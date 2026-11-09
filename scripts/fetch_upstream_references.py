"""Clone or fast-forward the upstream SMBIOS reference checkouts under ``c-versions/``.

Reads ``scripts/upstream_references.json`` (the single source of truth for the
upstream URLs - nothing is hard-coded here) and, for every entry, makes sure a
**full** (non-shallow) clone of that upstream's default branch exists at the
entry's ``path``:

- ``path`` absent: ``git clone --no-recurse-submodules -- <url> <path>``.
- ``path`` is an existing git work tree (not a symlink) whose
  ``remote.origin.url`` equals the manifest URL exactly: fetch, then
  fast-forward the checked-out branch from its upstream (``git pull
  --ff-only`` semantics). A dirty tree, local commits, a detached HEAD or a
  missing upstream branch is reported as a failure and the checkout is left
  untouched.
- ``path`` is a symlink, an existing non-git file/directory, or a clone whose
  origin URL differs from the manifest: warn and skip. ``c-versions/`` itself
  being a symlink, or a ``path`` whose parent resolves outside
  ``c-versions/``, is likewise skipped. The script never deletes, moves or
  overwrites anything.

Every git invocation runs with ``core.fsmonitor=false``, ``core.hooksPath``
pointed at the null device, and only the ``https`` transport allowed, so a
pre-existing clone's ``.git/config`` cannot make this script execute an
fsmonitor command, a hook, or a non-HTTPS transport helper.

After each entry it prints ``<name> <path> <HEAD SHA>`` (or ``skipped`` /
``failed`` in place of the SHA). The checkouts float at upstream HEAD; the
manifest's ``baseline_sha`` (the last triaged upstream commit) is metadata
only and is never checked out.

License isolation: both upstreams are GPL-2.0-or-later. ``c-versions/`` is
git-ignored and must never be tracked, built, or linked. Every manifest
``path`` is therefore required to live under ``c-versions/``, so a malformed
manifest cannot clone GPL code into a tracked location.

Usage::

    python scripts/fetch_upstream_references.py
    python scripts/fetch_upstream_references.py --manifest other.json

Requires ``git`` on ``PATH`` and network access. This is a developer/agent
tool; neither CI nor the build invokes it.

Exit status is ``1`` when any entry failed (or the manifest is invalid), else
``0``. A skipped entry is a warning, not a failure.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Optional
from urllib.parse import urlsplit

REPO_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_MANIFEST = REPO_ROOT / "scripts" / "upstream_references.json"

# The only directory upstream clones may be placed in. It is git-ignored
# (``/c-versions/`` in .gitignore), which is what keeps the GPL sources out
# of this repository.
REFERENCE_DIR = "c-versions"

_REQUIRED_FIELDS = ("name", "path", "url", "baseline_sha")

# Prepended to every git invocation. The clones under c-versions/ are
# third-party trees whose .git/config this script did not necessarily write,
# so config keys that execute commands are neutralised, and only the https
# transport is permitted (no ext::, file://, ssh:// helpers). os.devnull keeps
# the hooks path valid on Windows ("nul") as well as POSIX ("/dev/null").
_GIT_SAFETY_OPTIONS = (
    "-c", "core.fsmonitor=false",
    "-c", f"core.hooksPath={os.devnull}",
    "-c", "protocol.allow=never",
    "-c", "protocol.https.allow=always",
)


class ManifestError(Exception):
    """Raised when the manifest is missing, unreadable or malformed."""


@dataclass(frozen=True)
class Upstream:
    """One manifest entry.

    :ivar name: short identifier, e.g. ``dmidecode_linux``.
    :ivar path: repo-root-relative POSIX path of the clone, under ``c-versions/``.
    :ivar url: HTTPS clone URL.
    :ivar baseline_sha: last triaged upstream commit (metadata only).
    """

    name: str
    path: str
    url: str
    baseline_sha: str


def _validate_path(raw: str) -> None:
    """Reject a manifest ``path`` that does not stay inside ``c-versions/``.

    The check is purely lexical (no symlink resolution), so it is safe to run
    before the target exists.

    :param raw: the ``path`` string from the manifest.
    :raises ManifestError: if the path is absolute, contains ``..``, or is not
        a strict descendant of ``c-versions/``.
    """
    pure = PurePosixPath(raw)
    if pure.is_absolute() or "\\" in raw or ":" in raw:
        raise ManifestError(f"path {raw!r} must be a relative POSIX path")
    parts = [p for p in pure.parts if p != "."]
    if ".." in parts or len(parts) < 2 or parts[0] != REFERENCE_DIR:
        raise ManifestError(f"path {raw!r} must be inside {REFERENCE_DIR}/")


def load_manifest(manifest_path: Path) -> list[Upstream]:
    """Parse and validate the upstream manifest.

    :param manifest_path: path to the JSON manifest.
    :returns: the entries, in manifest order.
    :raises ManifestError: on any I/O, JSON or schema problem.
    """
    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ManifestError(f"cannot read {manifest_path}: {exc}") from exc

    entries = data.get("upstreams") if isinstance(data, dict) else None
    if not isinstance(entries, list) or not entries:
        raise ManifestError(f"{manifest_path}: expected a non-empty 'upstreams' list")

    upstreams: list[Upstream] = []
    seen_names: set[str] = set()
    seen_paths: set[str] = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise ManifestError(f"upstreams[{index}] is not an object")
        missing = [f for f in _REQUIRED_FIELDS if not isinstance(entry.get(f), str) or not entry[f]]
        if missing:
            raise ManifestError(f"upstreams[{index}] missing/empty field(s): {', '.join(missing)}")
        if not entry["url"].startswith("https://"):
            raise ManifestError(f"upstreams[{index}] url must be HTTPS: {entry['url']!r}")
        if "@" in urlsplit(entry["url"]).netloc:
            raise ManifestError(f"upstreams[{index}] url must not contain userinfo (user@host)")
        _validate_path(entry["path"])
        if entry["name"] in seen_names or entry["path"] in seen_paths:
            raise ManifestError(f"upstreams[{index}] duplicates an earlier name or path")
        seen_names.add(entry["name"])
        seen_paths.add(entry["path"])
        upstreams.append(Upstream(entry["name"], entry["path"], entry["url"], entry["baseline_sha"]))
    return upstreams


def _git(args: list[str], cwd: Optional[Path] = None) -> subprocess.CompletedProcess:
    """Run ``git`` with an argument list (no shell) and capture its output.

    Terminal credential prompts are disabled so an unreachable/private URL
    fails instead of hanging, and ``_GIT_SAFETY_OPTIONS`` is prepended so no
    clone-local config can run an fsmonitor command, a hook, or a non-HTTPS
    transport.

    :param args: arguments after ``git``.
    :param cwd: working directory, or ``None`` for the current one.
    :returns: the completed process (never raises on a non-zero exit).
    """
    env = dict(os.environ, GIT_TERMINAL_PROMPT="0")
    return subprocess.run(
        ["git", *_GIT_SAFETY_OPTIONS, *args],
        cwd=str(cwd) if cwd is not None else None,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )


def _warn(message: str) -> None:
    """Print a warning to stderr."""
    print(f"warning: {message}", file=sys.stderr)


def _git_error(proc: subprocess.CompletedProcess) -> str:
    """Return the most useful single line of a failed git command's output."""
    text = (proc.stderr or proc.stdout or "").strip()
    return text.splitlines()[-1] if text else f"exit status {proc.returncode}"


def _is_own_work_tree(target: Path) -> bool:
    """Return whether ``target`` is the top level of its own git work tree.

    A plain directory inside ``c-versions/`` would otherwise be reported as
    part of *this* repository's work tree by ``git rev-parse``, so the
    reported top level must be ``target`` itself.
    """
    proc = _git(["rev-parse", "--show-toplevel"], cwd=target)
    if proc.returncode != 0:
        return False
    try:
        return Path(proc.stdout.strip()).resolve() == target.resolve()
    except OSError:
        return False


def _head_sha(target: Path) -> Optional[str]:
    """Return the full HEAD SHA of the checkout at ``target``, or ``None``."""
    proc = _git(["rev-parse", "HEAD"], cwd=target)
    return proc.stdout.strip() if proc.returncode == 0 else None


def _is_inside(path: Path, directory: Path) -> bool:
    """Return whether resolved ``path`` is ``directory`` or a descendant of it."""
    try:
        path.resolve().relative_to(directory.resolve())
    except (OSError, ValueError):
        return False
    return True


def _origin_url(target: Path) -> Optional[str]:
    """Return the clone's ``remote.origin.url``, or ``None`` if unset."""
    proc = _git(["config", "--get", "remote.origin.url"], cwd=target)
    return proc.stdout.strip() if proc.returncode == 0 else None


def _clone(upstream: Upstream, target: Path) -> Optional[str]:
    """Make a full clone of the upstream's default branch at ``target``.

    Submodules are never cloned (``--no-recurse-submodules``), and ``--``
    stops the URL from ever being parsed as an option.

    :returns: the new HEAD SHA, or ``None`` on failure (already reported).
    """
    target.parent.mkdir(parents=True, exist_ok=True)
    proc = _git(["clone", "--quiet", "--no-recurse-submodules", "--", upstream.url, str(target)])
    if proc.returncode != 0:
        _warn(f"{upstream.name}: git clone failed: {_git_error(proc)}")
        return None
    return _head_sha(target)


def _fast_forward(upstream: Upstream, target: Path) -> Optional[str]:
    """Fetch and fast-forward an existing clone without ever discarding work.

    :returns: the resulting HEAD SHA, or ``None`` if the checkout could not
        be fast-forwarded (reported, and left untouched).
    """
    name = upstream.name

    def fail(reason: str) -> None:
        _warn(f"{name}: {reason}; leaving {upstream.path} untouched")

    proc = _git(["fetch", "--quiet", "--prune"], cwd=target)
    if proc.returncode != 0:
        fail(f"git fetch failed: {_git_error(proc)}")
        return None
    if _git(["symbolic-ref", "--quiet", "HEAD"], cwd=target).returncode != 0:
        fail("detached HEAD")
        return None
    proc = _git(["status", "--porcelain", "--untracked-files=no"], cwd=target)
    if proc.returncode != 0 or proc.stdout.strip():
        fail("working tree has uncommitted changes")
        return None
    if _git(["rev-parse", "--verify", "--quiet", "@{upstream}"], cwd=target).returncode != 0:
        fail("current branch has no upstream branch")
        return None
    proc = _git(["rev-list", "--count", "@{upstream}..HEAD"], cwd=target)
    if proc.returncode != 0 or proc.stdout.strip() != "0":
        fail("current branch has local commits not on its upstream")
        return None
    proc = _git(["merge", "--ff-only", "--quiet", "@{upstream}"], cwd=target)
    if proc.returncode != 0:
        fail(f"fast-forward failed: {_git_error(proc)}")
        return None
    return _head_sha(target)


def sync_upstream(upstream: Upstream, root: Path = REPO_ROOT) -> str:
    """Clone the upstream, or fast-forward its checked-out branch from its upstream.

    :param upstream: the manifest entry.
    :param root: directory the entry's ``path`` is relative to.
    :returns: the HEAD SHA, ``"skipped"`` or ``"failed"``.
    """
    reference_dir = root / REFERENCE_DIR
    target = root.joinpath(*PurePosixPath(upstream.path).parts)

    # A symlinked c-versions/ could point anywhere, including a tracked
    # directory; refuse rather than write through it.
    if reference_dir.is_symlink():
        _warn(f"{upstream.name}: {REFERENCE_DIR}/ is a symlink; skipping (not modified)")
        return "skipped"
    # Symlink check first: is_dir()/exists() follow links. A broken symlink
    # also counts as "present" and is skipped, never replaced.
    if target.is_symlink():
        _warn(f"{upstream.name}: {upstream.path} is a symlink; skipping (not modified)")
        return "skipped"
    # The manifest path is lexically inside c-versions/, but an intermediate
    # directory symlink could still redirect it elsewhere.
    if not _is_inside(target.parent, reference_dir):
        _warn(f"{upstream.name}: {upstream.path} resolves outside {REFERENCE_DIR}/; skipping (not modified)")
        return "skipped"
    if not target.exists():
        return _clone(upstream, target) or "failed"
    if not target.is_dir() or not _is_own_work_tree(target):
        _warn(f"{upstream.name}: {upstream.path} exists but is not a git clone; skipping (not modified)")
        return "skipped"
    origin = _origin_url(target)
    if origin != upstream.url:
        # The local URL is deliberately not echoed: it may embed credentials.
        _warn(
            f"{upstream.name}: {upstream.path} remote.origin.url does not match the manifest URL "
            f"{upstream.url}; skipping (not modified)"
        )
        return "skipped"
    return _fast_forward(upstream, target) or "failed"


def main(argv: Optional[list[str]] = None) -> int:
    """Command-line entry point.

    :param argv: arguments (default: ``sys.argv[1:]``).
    :returns: process exit status.
    """
    parser = argparse.ArgumentParser(
        description="Clone or fast-forward the upstream reference checkouts under c-versions/."
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        help="Manifest to read (default: scripts/upstream_references.json). "
        "Entry paths always resolve against the repo root.",
    )
    args = parser.parse_args(argv)

    try:
        upstreams = load_manifest(args.manifest)
    except ManifestError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    failed = False
    for upstream in upstreams:
        try:
            result = sync_upstream(upstream)
        except OSError as exc:  # e.g. git not on PATH, permission denied
            _warn(f"{upstream.name}: {exc}")
            result = "failed"
        failed = failed or result == "failed"
        print(f"{upstream.name} {upstream.path} {result}", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
