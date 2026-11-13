#!/usr/bin/env python3
"""Version-consistency check (milestone 0014 plan SC3; ADR 0005 D5; Story 03.0 Task 01).

Reads :data:`SITES_FILE` (``scripts/ci/version_sites.json``) and checks every
listed version site against the one authoritative value, the root
``CMakeLists.txt`` ``project(... VERSION X.Y.Z)``:

- ``equal`` sites must hold a literal equal to the authoritative value;
- ``derived`` sites must reference ``${PROJECT_VERSION}`` (or
  ``${CMAKE_PROJECT_VERSION}``). While the root VERSION does not exist yet
  (it is milestone 0012's product-build change), the file's
  ``interim_authoritative`` site stands in for it and literals are accepted,
  so with today's single site the check passes trivially (plan SC3
  "Prerequisite").
- ``--tag <name>`` (tag events; ``stage.py lint`` passes ``$CI_REF_NAME``):
  a tag starting with ``v`` must be ``v<X.Y.Z>[-<pre>]`` with ``X.Y.Z`` equal
  to the authoritative value. Any other tag (for example the legacy
  ``-DGIT_TAG`` scheme) is not a release and is only noted.

Output follows RC1: one ``ok: versions: ...`` line, or one failure annotation
whose first diagnostic is the first discrepancy, ``<site>: expected <x> ->
actual <y>``, followed by the others (at most 20, plus ``(+k more)``). Every
value shown passes an allowlist (``<invalid>`` otherwise, CR-4).

Usage::

    python3 scripts/ci/check_versions.py [--tag NAME] [--repo-root DIR] [--sites FILE]

Exit codes: 0 consistent, 1 a mismatch, 2 usage error or unreadable/invalid
sites file. Python 3 standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import json  # noqa: E402
import re  # noqa: E402
import time  # noqa: E402
from dataclasses import dataclass  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Any, Final, List, Optional, Sequence, Tuple  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import Reporter, get_reporter  # noqa: E402

REPO_ROOT: Final = Path(_HERE).resolve().parents[1]
SITES_FILE: Final = Path(_HERE) / "version_sites.json"
LABEL: Final = "versions"
RULES: Final = ("equal", "derived")
#: A version value: X.Y.Z with an optional pre-release suffix.
VERSION: Final = re.compile(r"^\d{1,6}\.\d{1,6}\.\d{1,6}(?:-[0-9A-Za-z.]{1,40})?$")
#: A release tag (ADR 0005 D5).
RELEASE_TAG: Final = re.compile(r"^v(\d{1,6}\.\d{1,6}\.\d{1,6})(?:-[0-9A-Za-z.]{1,40})?$")
#: A reference that makes a site derived from the authoritative value.
DERIVED_REF: Final = re.compile(r"^\$\{(?:CMAKE_)?PROJECT_VERSION\}$")
#: Marker returned by an extractor for a derived reference.
DERIVED: Final = "${PROJECT_VERSION}"
_SHOW: Final = re.compile(r"^[0-9A-Za-z.+_-]{1,40}$")
_MAX_LINES: Final = 20


class SitesError(ValueError):
    """The sites data file is unreadable or malformed (exit 2)."""


@dataclass(frozen=True)
class Site:
    """One row of :data:`SITES_FILE`."""

    site: str
    file: str
    extractor: str
    rule: str = "equal"


def _show(value: Optional[str]) -> str:
    """Return ``value`` if safe to print outside a fence (CR-4)."""
    if value is None:
        return "missing"
    if value == DERIVED:
        return "${PROJECT_VERSION}"
    if value.startswith("${"):
        return "another variable"
    return value if _SHOW.match(value) else "<invalid>"


def discrepancy(what: str, expected: Optional[str], actual: Optional[str]) -> str:
    """Return one RC1 discrepancy line, ``<what>: expected <x> -> actual <y>``."""
    return f"{what}: expected {_show(expected)} -> actual {_show(actual)}"[:160]


def strip_cmake_comments(text: str) -> str:
    """Return ``text`` without CMake ``#`` line comments and ``#[[ ]]`` bracket comments.

    A ``#`` inside a double-quoted string is kept.
    """
    text = re.sub(r"#\[(=*)\[.*?\]\1\]", "", text, flags=re.DOTALL)
    out: List[str] = []
    for line in text.split("\n"):
        quoted = False
        cut = len(line)
        for i, ch in enumerate(line):
            if ch == '"' and (i == 0 or line[i - 1] != "\\"):
                quoted = not quoted
            elif ch == "#" and not quoted:
                cut = i
                break
        out.append(line[:cut])
    return "\n".join(out)


def _read(repo_root: Path, rel: str) -> str:
    path = (repo_root / rel)
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def extract(repo_root: Path, site: Site) -> Optional[str]:
    """Return the version a site holds: a literal, :data:`DERIVED`, another ``${...}``, or ``None``.

    Raises:
        SitesError: The extractor name is unknown.
    """
    kind, _, arg = site.extractor.partition(":")
    if kind == "cmake_project_version":
        text = strip_cmake_comments(_read(repo_root, site.file))
        for call in re.finditer(r"\bproject\s*\(([^)]*)\)", text, flags=re.IGNORECASE):
            match = re.search(r"\bVERSION\s+\"?([^\s\")]+)\"?", call.group(1))
            if match:
                return match.group(1)
        return None
    if kind == "cmake_set":
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,63}", arg):
            raise SitesError(f"bad cmake_set variable in site {site.site!r}")
        text = strip_cmake_comments(_read(repo_root, site.file))
        match = re.search(r"\bset\s*\(\s*" + re.escape(arg) + r"\s+\"?([^\s\")]+)\"?",
                          text, flags=re.IGNORECASE)
        if match is None:
            return None
        value = match.group(1)
        return DERIVED if DERIVED_REF.match(value) else value
    if kind == "json_key":
        try:
            node: Any = json.loads(_read(repo_root, site.file) or "null")
        except ValueError:
            return None
        for key in arg.split("."):
            node = node.get(key) if isinstance(node, dict) else None
        return node if isinstance(node, str) else None
    raise SitesError(f"unknown extractor {kind!r} in site {site.site!r}")


def load_sites(path: Path) -> Tuple[Site, Optional[str], List[Site]]:
    """Return ``(authoritative, interim site name, sites)`` from the data file.

    Raises:
        SitesError: The file is unreadable, not the expected shape, or a rule is unknown.
    """
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise SitesError(f"cannot read {path.name} ({type(exc).__name__})") from None

    def site(entry: Any, default_rule: str = "equal") -> Site:
        if not isinstance(entry, dict) or not all(
                isinstance(entry.get(k), str) for k in ("site", "file", "extractor")):
            raise SitesError(f"{path.name}: each site needs string site/file/extractor")
        rule = entry.get("rule", default_rule)
        if rule not in RULES:
            raise SitesError(f"{path.name}: unknown rule {rule!r}")
        return Site(entry["site"], entry["file"], entry["extractor"], rule)

    if not isinstance(data, dict) or not isinstance(data.get("sites"), list):
        raise SitesError(f"{path.name}: expected an object with a 'sites' list")
    interim = data.get("interim_authoritative")
    if interim is not None and not isinstance(interim, str):
        raise SitesError(f"{path.name}: interim_authoritative must be a site name")
    sites = [site(e) for e in data["sites"]]
    if interim is not None and interim not in {s.site for s in sites}:
        raise SitesError(f"{path.name}: interim_authoritative names no listed site")
    return site(data.get("authoritative")), interim, sites


@dataclass(frozen=True)
class Outcome:
    """Result of :func:`check`."""

    authority: Optional[str]
    authority_site: str
    interim: bool
    problems: List[str]
    notes: List[str]
    sites: int


def check(repo_root: Path, sites_file: Path = SITES_FILE, tag: Optional[str] = None) -> Outcome:
    """Check every site (and the tag) against the authoritative value.

    Raises:
        SitesError: Invalid data file (see :func:`load_sites`).
    """
    authoritative, interim_name, sites = load_sites(sites_file)
    problems: List[str] = []
    notes: List[str] = []
    value = extract(repo_root, authoritative)
    interim = value is None and interim_name is not None
    authority_site = authoritative.site
    if interim:
        stand_in = next(s for s in sites if s.site == interim_name)
        value = extract(repo_root, stand_in)
        authority_site = stand_in.site
    if value is None or not VERSION.match(value):
        problems.append(discrepancy(f"{authority_site} (authoritative)", "X.Y.Z", value))
        return Outcome(None, authority_site, interim, problems, notes, len(sites))

    for s in sites:
        actual = extract(repo_root, s)
        if actual == DERIVED and s.rule == "derived":
            continue
        if s.rule == "derived" and not interim and actual is not None and VERSION.match(actual):
            problems.append(discrepancy(s.site, DERIVED, actual))  # hand-edited literal (D5)
        elif actual != value:
            problems.append(discrepancy(s.site, value, actual))

    if tag:
        if not tag.startswith("v"):
            notes.append("tag is not a v* release tag; not checked")
        else:
            match = RELEASE_TAG.match(tag)
            base = value.split("-", 1)[0]
            if match is None or match.group(1) != base:
                problems.append(discrepancy("tag", f"v{base}", tag))
    return Outcome(value, authority_site, interim, problems, notes, len(sites))


def run(repo_root: Path, *, sites_file: Path = SITES_FILE, tag: Optional[str] = None,
        reporter: Optional[Reporter] = None, results: Optional[Path] = None) -> int:
    """Run :func:`check`, print the RC1 output, append the RC3 record; return 0, 1 or 2."""
    reporter = reporter or get_reporter(None)
    results = results or Path(env_names.get("CI_RESULTS_FILE", required=True))
    start = time.monotonic()
    try:
        outcome = check(repo_root, sites_file, tag)
    except SitesError as exc:
        ci_step.emit_error(f"check_versions.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    elapsed = round(time.monotonic() - start, 1)
    if outcome.problems:
        rest = outcome.problems[1:_MAX_LINES]
        more = len(outcome.problems) - 1 - len(rest)
        if more > 0:
            rest.append(f"(+{more} more)")
        ci_step.emit(reporter.step_fail(LABEL, 1, elapsed, outcome.problems[0], rest))
        ci_step.append_record(ci_step.StepResult(LABEL, "hard", "fail", 1, elapsed, "",
                                                 outcome.problems[0], None), results)
        return 1
    source = outcome.authority_site + (", interim" if outcome.interim else "")
    summary = f"{outcome.authority} ({source}), {outcome.sites} sites"
    if tag:
        summary += f", tag {_show(tag)}" + (" not a release" if outcome.notes else " ok")
    ci_step.emit(reporter.step_ok(LABEL, summary, elapsed))
    ci_step.append_record(ci_step.StepResult(LABEL, "hard", "ok", 0, elapsed, summary, None,
                                             None), results)
    return 0


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="check_versions.py",
                                description="Check every version site against the authority.")
    p.add_argument("--tag", default=None, help="tag name of a tag event (CI_REF_NAME)")
    p.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    p.add_argument("--sites", type=Path, default=SITES_FILE)
    p.add_argument("--reporter", default=None)
    return p


def main(argv: Sequence[str]) -> int:
    """Command-line entry point."""
    args = _parser().parse_args(list(argv))
    try:
        reporter = get_reporter(args.reporter)
    except ValueError as exc:
        ci_step.emit_error(f"check_versions.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    return run(args.repo_root, sites_file=args.sites, tag=args.tag, reporter=reporter)


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step.emit([f"error: check_versions.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
