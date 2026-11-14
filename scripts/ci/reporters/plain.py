"""Plain-text reporter backend for local runs (ADR 0005; 0014 RC1).

Prints ``error:``/``warning:`` lines, needs no escaping and no fence (a
terminal does not interpret workflow commands), and prints the job summary to
stdout. This is the default backend. A job output (local runs have no job)
is printed as one ``output: <name>=<json>`` line.
"""

from __future__ import annotations

import sys
from typing import List

from reporters import (clean_text, fail_text, info_text, ok_line, output_json, skip_line,
                       warn_text)


class PlainReporter:
    """Reporter backend for a developer terminal."""

    name = "plain"

    def step_ok(self, label: str, summary: str, elapsed_s: float) -> List[str]:
        """Render the RC1 ``ok:`` line."""
        return [ok_line(label, summary, elapsed_s)]

    def step_fail(self, label: str, rc: int, elapsed_s: float, first_diag: str,
                  excerpt: List[str]) -> List[str]:
        """Render one ``error:`` line plus the excerpt."""
        return [f"error: {fail_text(label, rc, elapsed_s, first_diag)}"] + list(excerpt)

    def step_warn(self, label: str, reason: str, rc: int, excerpt: List[str]) -> List[str]:
        """Render one ``warning: best-effort:`` line plus the excerpt (RC5)."""
        return [f"warning: best-effort: {label}: {warn_text(reason, rc)}"] + list(excerpt)

    def step_skip(self, label: str, reason: str) -> List[str]:
        """Render the RC5 ``skip:`` line."""
        return [skip_line(label, reason)]

    def info(self, line: str) -> List[str]:
        """Render one sanitised informational line."""
        return [info_text(line)]

    def warning(self, title: str, text: str) -> List[str]:
        """Render one ``warning: <title>: <text>`` line (sanitised)."""
        return [f"warning: {clean_text(title)}: {clean_text(text)}"]

    def set_output(self, name: str, value: object) -> None:
        """Print ``output: <name>=<single-line JSON>`` (validated like every backend)."""
        sys.stdout.write(f"output: {name}={output_json(name, value)}\n")
        sys.stdout.flush()

    def begin_untrusted_block(self) -> List[str]:
        """No fence is needed on a terminal."""
        return []

    def end_untrusted_block(self) -> List[str]:
        """No fence is needed on a terminal."""
        return []

    def internal_error(self, what: str) -> List[str]:
        """Render a runner bug as one ``error:`` line."""
        return [f"error: ci_step: internal error ({what})"]

    def job_summary(self, markdown: str) -> None:
        """Print the summary markdown to stdout."""
        sys.stdout.write(markdown if markdown.endswith("\n") else markdown + "\n")
