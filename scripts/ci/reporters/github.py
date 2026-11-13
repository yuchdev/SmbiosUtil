"""GitHub Actions reporter backend (ADR 0005; 0014 RC1).

Renders failures as ``::error`` and best-effort failures as ``::warning``
workflow commands, escapes annotation text (``%``, CR, LF), fences every
untrusted excerpt with ``::stop-commands::<token>`` ... ``::<token>::`` using a
fresh ``secrets.token_hex(16)`` per fence (never derived from tool output),
writes the job summary to ``CI_SUMMARY_FILE`` and job outputs to
``CI_OUTPUT_FILE`` (single-line JSON, random heredoc delimiter; CR-6, CR-8).

Unfenced lines (``ok:``, ``skip:``, ``info``) need no fence here: GitHub
interprets a workflow command only at line start, and each of those lines
starts with a fixed alphanumeric prefix after ``safe_line`` (CR-4).
"""

from __future__ import annotations

import os
import secrets
import sys
from typing import List, Optional

import env_names
from reporters import (ANNOTATION_MAX, clean_text, fail_text, info_text, ok_line,
                       output_delimiter, output_json, skip_line, warn_text)

#: The runner's own file-command variables. GitHub exposes them only as
#: process environment, not as an expression context, and policy rule P7 bans
#: ``$VAR`` in a ``run:`` line, so the workflow adapter cannot map them onto
#: ``CI_OUTPUT_FILE``/``CI_SUMMARY_FILE``. This backend (the GitHub-specific
#: piece of the adapter, ADR 0005) falls back to them when the neutral name is
#: empty (milestone 0014 Story 03.0 Task 01). Children never see either name:
#: ``reporters.child_environment`` denies both (CR-5).
_PLATFORM_OUTPUT_VAR = "GITHUB_OUTPUT"
_PLATFORM_SUMMARY_VAR = "GITHUB_STEP_SUMMARY"


def _target(neutral: str, platform_var: str) -> str:
    """Return the neutral name's value, else the runner's own variable, else ``""``."""
    return env_names.get(neutral) or os.environ.get(platform_var, "")


def escape_annotation(text: str) -> str:
    """Escape workflow-command message text: cut to 160 chars, then ``%``, CR, LF.

    The cut happens first, so an escape sequence is never split.
    """
    return (text[:ANNOTATION_MAX]
            .replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A"))


def _escape_property(text: str) -> str:
    """Escape a workflow-command property value (adds ``:`` and ``,``)."""
    return escape_annotation(text).replace(":", "%3A").replace(",", "%2C")


class GithubReporter:
    """Reporter backend for GitHub Actions workflow commands."""

    name = "github"

    def __init__(self) -> None:
        self._token: Optional[str] = None

    def step_ok(self, label: str, summary: str, elapsed_s: float) -> List[str]:
        """Render the RC1 ``ok:`` line."""
        return [ok_line(label, summary, elapsed_s)]

    def step_fail(self, label: str, rc: int, elapsed_s: float, first_diag: str,
                  excerpt: List[str]) -> List[str]:
        """Render one ``::error`` annotation plus the fenced excerpt."""
        head = (f"::error title={_escape_property(label)}::"
                f"{escape_annotation(fail_text(label, rc, elapsed_s, first_diag))}")
        return [head] + self._fenced(excerpt)

    def step_warn(self, label: str, reason: str, rc: int, excerpt: List[str]) -> List[str]:
        """Render one RC5 ``::warning title=best-effort: ...`` line plus the fenced excerpt."""
        head = (f"::warning title=best-effort: {_escape_property(label)}::"
                f"{escape_annotation(warn_text(reason, rc))}")
        return [head] + self._fenced(excerpt)

    def step_skip(self, label: str, reason: str) -> List[str]:
        """Render the RC5 ``skip:`` line."""
        return [skip_line(label, reason)]

    def info(self, line: str) -> List[str]:
        """Render one sanitised informational line (never a workflow command)."""
        return [info_text(line)]

    def warning(self, title: str, text: str) -> List[str]:
        """Render one ``::warning title=<title>::<text>`` annotation (escaped, CR-4)."""
        return [f"::warning title={_escape_property(clean_text(title))}::"
                f"{escape_annotation(clean_text(text))}"]

    def set_output(self, name: str, value: object) -> None:
        """Append ``name<<DELIM`` / JSON / ``DELIM`` to ``CI_OUTPUT_FILE`` in one write.

        The delimiter is a fresh ``secrets.token_hex(16)`` value per call, and
        the JSON value is one line, so no value can end the entry early or
        forge a second output. The file is opened without following a
        symlink and, if created here, with mode 0600.

        The target is ``CI_OUTPUT_FILE``, else the runner's own output file
        (see :data:`_PLATFORM_OUTPUT_VAR`).

        Raises:
            ValueError: Invalid name or value (see ``reporters.output_json``).
            SystemExit: Neither ``CI_OUTPUT_FILE`` nor the runner's output
                file is set (exit 2, ADR 0005).
        """
        text = output_json(name, value)
        delimiter = output_delimiter(text)
        target = _target("CI_OUTPUT_FILE", _PLATFORM_OUTPUT_VAR)
        if not target:
            target = env_names.get("CI_OUTPUT_FILE", required=True)  # exits 2
        data = f"{name}<<{delimiter}\n{text}\n{delimiter}\n".encode("ascii")
        flags = (os.O_WRONLY | os.O_CREAT | os.O_APPEND | getattr(os, "O_NOFOLLOW", 0)
                 | getattr(os, "O_BINARY", 0))
        fd = os.open(target, flags, 0o600)
        try:
            if os.write(fd, data) != len(data):
                raise OSError("short write to the job-output file")
        finally:
            os.close(fd)

    def begin_untrusted_block(self) -> List[str]:
        """Stop workflow-command processing with a fresh random token."""
        self._token = secrets.token_hex(16)
        return [f"::stop-commands::{self._token}"]

    def end_untrusted_block(self) -> List[str]:
        """Resume workflow-command processing with the token from the opening fence."""
        token, self._token = self._token, None
        return [f"::{token}::"] if token else []

    def internal_error(self, what: str) -> List[str]:
        """Render a runner bug as ``::error title=ci_step::internal error (...)``."""
        return [f"::error title=ci_step::{escape_annotation(f'internal error ({what})')}"]

    def job_summary(self, markdown: str) -> None:
        """Append the markdown to ``CI_SUMMARY_FILE`` (else the runner's summary file); print it if neither is set."""
        target = _target("CI_SUMMARY_FILE", _PLATFORM_SUMMARY_VAR)
        if not target:
            sys.stdout.write(markdown if markdown.endswith("\n") else markdown + "\n")
            return
        with open(target, "a", encoding="utf-8") as fh:
            fh.write(markdown if markdown.endswith("\n") else markdown + "\n")

    def _fenced(self, excerpt: List[str]) -> List[str]:
        if not excerpt:
            return []
        return self.begin_untrusted_block() + list(excerpt) + self.end_untrusted_block()
