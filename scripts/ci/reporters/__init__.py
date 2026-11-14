"""Reporter backends for the neutral RC1 events (ADR 0005, "Platform portability").

``ci_step`` decides *what* to report (neutral events, already truncated,
bounded and sanitised); a backend decides only *how* it looks on one CI
platform. This package is the only ``scripts/ci/`` code that emits platform
workflow commands or service messages, or writes a platform summary file.

Backends: ``github``, ``teamcity``, ``plain`` (local runs, the default).
Selection: an explicit name, else ``$CI_REPORTER``, else ``plain``.

Every rendering method returns the lines to print (without newlines) so the
caller can enforce the RC1 ceiling on the final output and print it in one
write; only :meth:`Reporter.job_summary` has a side effect of its own.
"""

from __future__ import annotations

import json
import re
import secrets
from typing import Dict, Final, FrozenSet, List, Mapping, Protocol, Sequence, Tuple

import env_names


class Reporter(Protocol):
    """Renders neutral RC1 events for one CI platform (ADR 0005)."""

    name: str

    def step_ok(self, label: str, summary: str, elapsed_s: float) -> List[str]:
        """Render a passing step: the single RC1 ``ok:`` line."""
        ...

    def step_fail(self, label: str, rc: int, elapsed_s: float, first_diag: str,
                  excerpt: List[str]) -> List[str]:
        """Render a failing step: one annotation, then the fenced excerpt."""
        ...

    def step_warn(self, label: str, reason: str, rc: int, excerpt: List[str]) -> List[str]:
        """Render a failed best-effort step (RC5): one warning, then the fenced excerpt."""
        ...

    def step_skip(self, label: str, reason: str) -> List[str]:
        """Render a step not run in this environment (RC5): one ``skip:`` line."""
        ...

    def warning(self, title: str, text: str) -> List[str]:
        """Render one standalone warning annotation that is not a step outcome.

        Added by milestone 0014 Story 03.0 Task 01 for ``cli_format_check.py``'s
        ``empty table`` notice (inventory C07). ``title`` and ``text`` are
        sanitised (CR-4) and cut to 160 characters.
        """
        ...

    def begin_untrusted_block(self) -> List[str]:
        """Open a fence that stops the platform interpreting tool output (T-3)."""
        ...

    def end_untrusted_block(self) -> List[str]:
        """Close the fence opened by the matching :meth:`begin_untrusted_block`."""
        ...

    def internal_error(self, what: str) -> List[str]:
        """Render a runner bug (``ci_step`` exit 70) as one annotation line."""
        ...

    def job_summary(self, markdown: str) -> None:
        """Publish the job summary markdown the platform's way."""
        ...

    def info(self, line: str) -> List[str]:
        """Render one informational line (RC4 header, ``info:``), sanitised (CR-4).

        The result never starts with ``::`` or ``##[`` and, on TeamCity, never
        carries ``##teamcity[``.
        """
        ...

    def set_output(self, name: str, value: object) -> None:
        """Set job output ``name`` to ``value`` as single-line JSON (CR-6, CR-8).

        The only way any ``scripts/ci`` script sets a job output.

        Raises:
            ValueError: ``name`` does not match :data:`OUTPUT_NAME`, or
                ``value`` is not JSON-serialisable.
        """
        ...


#: Names a child process of the step runner never inherits (threat model
#: CR-5, 0011 R-6). The denylist is a security control: removing a name needs a
#: threat-model note. Matching is case-insensitive (Windows environments are).
CHILD_ENV_DENY_NAMES: Final[FrozenSet[str]] = frozenset({
    # Platform debug switches: verbose output may echo secrets or untrusted data.
    "ACTIONS_STEP_DEBUG",
    "ACTIONS_RUNNER_DEBUG",
    "RUNNER_DEBUG",
    # GitHub file-command channels: a child could set LD_PRELOAD/PATH, a step
    # output or the summary for later steps of the same job, past every fence.
    "GITHUB_ENV",
    "GITHUB_OUTPUT",
    "GITHUB_PATH",
    "GITHUB_STATE",
    "GITHUB_STEP_SUMMARY",
    # The neutral job-output channel (ADR 0005): set for one stage.py process
    # only, never inherited by its children.
    "CI_OUTPUT_FILE",
    # OIDC request token and the runtime/cache/results service credentials.
    "ACTIONS_ID_TOKEN_REQUEST_TOKEN",
    "ACTIONS_ID_TOKEN_REQUEST_URL",
    "ACTIONS_RUNTIME_TOKEN",
    "ACTIONS_RUNTIME_URL",
    "ACTIONS_CACHE_URL",
    "ACTIONS_RESULTS_URL",
    # TeamCity password-type parameters exported to the environment.
    *env_names.TEAMCITY_SECURE_NAMES,
})

#: Name prefixes a child never inherits (CR-5): publish credentials, cloud
#: session variables and the TeamCity adapter's password-type exports.
CHILD_ENV_DENY_PREFIXES: Final[Tuple[str, ...]] = (
    "CI_PUBLISH_",
    "AWS_",
    *env_names.TEAMCITY_SECURE_PREFIXES,
)

#: Kept for Story 01.0 callers: the original three debug switches.
CHILD_ENV_DENYLIST: Tuple[str, ...] = (
    "ACTIONS_STEP_DEBUG",
    "ACTIONS_RUNNER_DEBUG",
    "RUNNER_DEBUG",
)


def is_denied_child_name(name: str) -> bool:
    """Return whether ``name`` is on the CR-5 name or prefix denylist."""
    upper = name.upper()
    return upper in CHILD_ENV_DENY_NAMES or upper.startswith(CHILD_ENV_DENY_PREFIXES)


def child_environment(environ: Mapping[str, str], pass_env: Sequence[str] = ()) -> Dict[str, str]:
    """Return a copy of ``environ`` without denied names (threat model CR-5).

    Args:
        environ: The parent environment.
        pass_env: Denied names the caller needs in the child, listed by exact
            name (compared case-insensitively). A publish destination, for
            example, exchanges the OIDC token itself, writes it to a 0600 file
            under ``CI_TEMP_DIR`` and passes only ``AWS_WEB_IDENTITY_TOKEN_FILE``.

    Returns:
        A new mapping; ``environ`` is not modified.
    """
    allowed = {name.upper() for name in pass_env}
    return {k: v for k, v in environ.items()
            if k.upper() in allowed or not is_denied_child_name(k)}


#: RC1: annotation text and ``ok:`` summaries are cut to this many characters.
ANNOTATION_MAX = 160

#: Longest label rendered on an unfenced line (the CLI accepts at most 80).
LABEL_MAX = 80


#: TeamCity service-message prefix; built by concatenation so the P11 needle
#: appears only where a backend authors a message.
SERVICE_MESSAGE_PREFIX = "##" + "teamcity["


def neutralise_service_messages(line: str) -> str:
    """Break every TeamCity service-message prefix in ``line`` (CR-4).

    Applied to every unfenced line built from caller or tool text, on every
    backend, so ``##teamcity[`` appears only in a message a backend authored.
    """
    return line.replace(SERVICE_MESSAGE_PREFIX, "## teamcity[")


def _clean(text: str, width: int) -> str:
    """Return ``text`` through ``ci_step.safe_line`` (first line only, cut to ``width``)
    with any service-message prefix broken (CR-4)."""
    from ci_step import safe_line  # lazy: ci_step imports this package
    first = text.splitlines()[0] if text else ""
    return neutralise_service_messages(safe_line(first.encode("utf-8", errors="replace"), width))


def ok_line(label: str, summary: str, elapsed_s: float) -> str:
    """Return the RC1 success line, identical for every backend.

    ``label`` and ``summary`` pass through ``safe_line`` (CR-4); the line always
    starts with ``ok: `` and so can never begin a workflow command.
    """
    summary = _clean(summary, ANNOTATION_MAX)
    tail = f": {summary}" if summary else ""
    return f"ok: {_clean(label, LABEL_MAX)}{tail} ({elapsed_s:.1f}s)"


def skip_line(label: str, reason: str) -> str:
    """Return the RC5 skip line, identical for every backend (``safe_line`` applied, CR-4)."""
    return f"skip: {_clean(label, LABEL_MAX)}: {_clean(reason, ANNOTATION_MAX)}"


def fail_text(label: str, rc: int, elapsed_s: float, first_diag: str) -> str:
    """Return the neutral failure message body (before backend escaping)."""
    return f"{label} failed (exit {rc}, {elapsed_s:.1f}s): {first_diag[:ANNOTATION_MAX]}"


#: Width cap of an :func:`info_text` line; RC4's own ceiling (plan.md).
INFO_MAX = 240


def info_text(line: str) -> str:
    """Return ``line`` made safe to print outside any fence (threat model CR-4).

    Added by milestone 0014 Story 02.0 Task 01 for ``print_env.py``; Story 02.0
    Task 04 made it the ``info()`` contract of every backend. The text goes through
    ``ci_step.safe_line`` (ESC/C0/C1/bidi stripped, first line only, cut to
    :data:`INFO_MAX`), and a result that would start with ``:`` or ``#`` gets
    an ``info: `` prefix, so it can never begin a workflow command
    (``::x``, ``##[x]``) or a service message.
    """
    text = _clean(line, INFO_MAX)
    if text.lstrip().startswith((":", "#")):
        text = f"info: {text}"[:INFO_MAX]
    return text


def clean_text(text: str, width: int = ANNOTATION_MAX) -> str:
    """Return ``text`` as one sanitised line (``safe_line`` + service-message neutralised).

    Public form of the helper every backend uses for caller text (CR-4).
    """
    return _clean(text, width)


def warn_text(reason: str, rc: int) -> str:
    """Return the neutral RC5 best-effort warning body (before backend escaping)."""
    return f"{reason[:ANNOTATION_MAX]} (exit {rc})"


#: A job-output name (CR-6): lower-case identifier, at most 64 characters.
OUTPUT_NAME = re.compile(r"^[a-z][a-z0-9_]{0,63}$")


def output_json(name: str, value: object) -> str:
    """Validate ``name`` and return ``value`` as single-line JSON (CR-6, CR-8).

    ``ensure_ascii`` escapes every non-ASCII character (including U+2028/9),
    and JSON escapes CR and LF inside strings, so the result is one line.

    Raises:
        ValueError: ``name`` does not match :data:`OUTPUT_NAME`, or ``value``
            cannot be serialised.
    """
    if not isinstance(name, str) or not OUTPUT_NAME.fullmatch(name):  # no trailing LF
        raise ValueError(f"invalid job-output name {name!r} (expected {OUTPUT_NAME.pattern})")
    try:
        text = json.dumps(value, ensure_ascii=True, separators=(",", ":"), sort_keys=True,
                          allow_nan=False)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"job output {name} is not JSON-serialisable ({type(exc).__name__})")
    if "\r" in text or "\n" in text:  # cannot happen with json.dumps; checked anyway
        raise ValueError(f"job output {name} is not a single line")
    return text


def output_delimiter(value_line: str) -> str:
    """Return a fresh heredoc delimiter from ``secrets.token_hex(16)`` that ``value_line`` lacks."""
    while True:
        delimiter = f"ci_output_{secrets.token_hex(16)}"
        if delimiter not in value_line:
            return delimiter


BACKEND_NAMES: Tuple[str, ...] = ("github", "teamcity", "plain")


def get_reporter(name: str | None) -> Reporter:
    """Return the backend called ``name``; ``None`` means ``$CI_REPORTER``, then ``plain``.

    Raises:
        ValueError: The name is not one of :data:`BACKEND_NAMES`.
    """
    chosen = name or env_names.get("CI_REPORTER") or "plain"
    if chosen == "github":
        from reporters.github import GithubReporter
        return GithubReporter()
    if chosen == "teamcity":
        from reporters.teamcity import TeamcityReporter
        return TeamcityReporter()
    if chosen == "plain":
        from reporters.plain import PlainReporter
        return PlainReporter()
    raise ValueError(f"unknown reporter {chosen!r} (expected one of {', '.join(BACKEND_NAMES)})")
