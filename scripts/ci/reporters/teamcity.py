"""TeamCity reporter backend (ADR 0005; 0014 RC1).

Renders failures as ``##teamcity[buildProblem ...]`` and best-effort failures
as ``##teamcity[message ... status='WARNING']`` service messages with TeamCity
``|`` escaping, fences every untrusted excerpt with
``##teamcity[disableServiceMessages]`` ... ``##teamcity[enableServiceMessages]``,
publishes the job summary as a ``ci-summary.md`` artifact plus a one-line
``buildStatus``, and sets job outputs with ``setParameter`` (CR-6, CR-8).

The fence covers only the tool excerpt. Every other line this backend prints
that it did not author itself (``ok:``, ``skip:``, ``info`` lines, the RC4
header, ``first_diagnostic`` inside ``buildProblem``) passes through
:func:`_neutralise`, so ``##teamcity[`` appears only in a message the backend
wrote (threat model CR-4: mid-line service-message behaviour is undocumented).

Unused until a migration happens; kept honest by unit tests on rendered strings
(ADR 0005 Consequences).
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import List, Optional

import env_names
from reporters import (ANNOTATION_MAX, clean_text, fail_text, info_text,
                       neutralise_service_messages, ok_line, output_json, skip_line, warn_text)

_ESCAPES = (
    ("|", "||"), ("'", "|'"), ("\n", "|n"), ("\r", "|r"), ("[", "|["), ("]", "|]"),
    ("\u0085", "|x"), (" ", "|l"), (" ", "|p"),
)

#: TeamCity truncates buildProblem identities longer than this.
_IDENTITY_MAX = 60


def escape_value(text: str, width: Optional[int] = ANNOTATION_MAX) -> str:
    """Escape a service-message attribute value; cut to ``width`` (160) chars first.

    ``width=None`` keeps the whole value (job outputs, which must not be cut).
    """
    out = text if width is None else text[:width]
    for raw, esc in _ESCAPES:  # "|" is first, so later escapes are not doubled
        out = out.replace(raw, esc)
    return out


def _neutralise(line: str) -> str:
    """Break any service-message prefix in a line this backend did not author.

    Applied inside the fence (defence in depth: a hostile line could otherwise
    contain ``##teamcity[enableServiceMessages]`` and close the fence early)
    and to every unfenced line built from caller or tool text (CR-4).
    """
    return neutralise_service_messages(line)


class TeamcityReporter:
    """Reporter backend for TeamCity service messages."""

    name = "teamcity"

    def step_ok(self, label: str, summary: str, elapsed_s: float) -> List[str]:
        """Render the RC1 ``ok:`` line, neutralised (CR-4)."""
        return [_neutralise(ok_line(label, summary, elapsed_s))]

    def step_fail(self, label: str, rc: int, elapsed_s: float, first_diag: str,
                  excerpt: List[str]) -> List[str]:
        """Render one ``buildProblem`` plus the fenced excerpt."""
        head = (f"##teamcity[buildProblem description='"
                f"{escape_value(fail_text(label, rc, elapsed_s, _neutralise(first_diag)))}' "
                f"identity='{escape_value(label[:_IDENTITY_MAX])}']")
        return [head] + self._fenced(excerpt)

    def step_warn(self, label: str, reason: str, rc: int, excerpt: List[str]) -> List[str]:
        """Render one WARNING ``message`` plus the fenced excerpt (RC5)."""
        text = _neutralise(f"best-effort: {label}: {warn_text(reason, rc)}")
        head = f"##teamcity[message text='{escape_value(text)}' status='WARNING']"
        return [head] + self._fenced(excerpt)

    def step_skip(self, label: str, reason: str) -> List[str]:
        """Render the RC5 ``skip:`` line, neutralised (CR-4)."""
        return [_neutralise(skip_line(label, reason))]

    def info(self, line: str) -> List[str]:
        """Render one sanitised informational line; any service-message prefix is broken."""
        return [_neutralise(info_text(line))]

    def warning(self, title: str, text: str) -> List[str]:
        """Render one WARNING ``message`` (escaped, neutralised; CR-4)."""
        body = f"{clean_text(title)}: {clean_text(text)}"
        return [f"##teamcity[message text='{escape_value(body)}' status='WARNING']"]

    def begin_untrusted_block(self) -> List[str]:
        """Disable service-message processing."""
        return ["##teamcity[disableServiceMessages]"]

    def end_untrusted_block(self) -> List[str]:
        """Re-enable service-message processing."""
        return ["##teamcity[enableServiceMessages]"]

    def internal_error(self, what: str) -> List[str]:
        """Render a runner bug as one ``buildProblem`` with identity ``ci_step``."""
        return [f"##teamcity[buildProblem description='"
                f"{escape_value(f'internal error ({what})')}' identity='ci_step']"]

    def job_summary(self, markdown: str) -> None:
        """Write ``ci-summary.md`` under ``CI_ARTIFACT_DIR``, publish it, set the build status."""
        directory = Path(env_names.get("CI_ARTIFACT_DIR", required=True))
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / "ci-summary.md"
        path.write_text(markdown if markdown.endswith("\n") else markdown + "\n", encoding="utf-8")
        first = next((ln.strip() for ln in markdown.splitlines() if ln.strip()), "")
        published = escape_value(_neutralise(str(path)))
        sys.stdout.write(f"##teamcity[publishArtifacts '{published}']\n")
        if first:
            status = escape_value(_neutralise(first))
            sys.stdout.write(f"##teamcity[buildStatus text='{status}']\n")
        sys.stdout.flush()

    def set_output(self, name: str, value: object) -> None:
        """Print one ``##teamcity[setParameter name='<name>' value='<json>']`` line.

        The value is single-line JSON, ``|``-escaped and never cut, so it
        cannot close the message or start a second one.

        Raises:
            ValueError: Invalid name or value (see ``reporters.output_json``).
        """
        text = output_json(name, value)
        sys.stdout.write(f"##teamcity[setParameter name='{name}' "
                         f"value='{escape_value(text, None)}']\n")
        sys.stdout.flush()

    def _fenced(self, excerpt: List[str]) -> List[str]:
        if not excerpt:
            return []
        body = [_neutralise(line) for line in excerpt]
        return self.begin_untrusted_block() + body + self.end_untrusted_block()
