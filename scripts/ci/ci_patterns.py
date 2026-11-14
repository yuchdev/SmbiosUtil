"""Named diagnostic pattern sets for ``ci_step.py`` (milestone 0014, RC1/RC2).

Each set is a tuple of compiled regexes matched with ``search()`` against one
already-sanitised log line. A line is a diagnostic line when any regex of any
selected set matches it. The sets match diagnostic *shapes*, never a bare word
such as ``error``, which would also match file names like
``memory_error_information_*.cpp``.

Contract: ``docs/roadmap/0014-ci-rework-from-scratch/plan.md`` (RC1, RC2).
"""

from __future__ import annotations

import re
from typing import Dict, Iterable, List, Pattern, Tuple


def _c(*patterns: str) -> Tuple[Pattern[str], ...]:
    return tuple(re.compile(p) for p in patterns)


PATTERN_SETS: Dict[str, Tuple[Pattern[str], ...]] = {
    # GCC/Clang/MSVC compiler and linker output (milestone 0011's run_quiet set).
    "cxx": _c(r"error:", r"error C[0-9]+", r"fatal error", r"undefined reference",
              r"linker command failed", r"LNK[0-9]+"),
    # CMake configure/generate output.
    "cmake": _c(r"CMake Error"),
    # vcpkg manifest-install output. NOT yet confirmed against a captured real
    # vcpkg failure log (Task 02 open item) - confirm before freezing.
    "vcpkg": _c(r"error:", r"BUILD_FAILED", r"-- Building .* failed",
                r"See logs for more information"),
    # CTest summary and per-test status lines.
    "ctest": _c(r"The following tests FAILED", r"\*\*\*Failed", r"\*\*\*Timeout",
                r"\*\*\*Exception", r"Not Run"),
    # Boost.Test assertion/fatal reports.
    "boost": _c(r'error: in "', r'fatal error: in "'),
    # AddressSanitizer / LeakSanitizer / UBSan reports.
    "sanitizer": _c(r"ERROR: AddressSanitizer", r"ERROR: LeakSanitizer",
                    r"runtime error:", r"SUMMARY: (Address|Undefined|Leak)Sanitizer"),
    # CPython tracebacks, unittest failures and argparse-style errors.
    "python": _c(r"Traceback", r"^FAIL:", r"^error:"),
    # apt-get / resolver failures.
    "apt": _c(r"^E: ", r"Could not resolve", r"Temporary failure resolving"),
    # Policy-gate discrepancy lines (check_workflow_policy.py, Story 02.0 Task 03).
    "policy": _c(r": (?:P[0-9]+|yaml): expected .* -> actual ", r"^\(\+[0-9]+ more\)$",
                 r"^error: "),
    # actionlint's default output, <file>:<line>:<col>: <message> [<rule>].
    "actionlint": _c(r"^\S+:[0-9]+:[0-9]+: ", r"^error: "),
}

#: Every set name, in declaration order; the RC2 default for ``--patterns``.
ALL_SETS: Tuple[str, ...] = tuple(PATTERN_SETS)


def compile_sets(names: Iterable[str]) -> List[Pattern[str]]:
    """Return the union of the named sets' regexes, de-duplicated, in order.

    Args:
        names: Set names from :data:`PATTERN_SETS`.

    Raises:
        KeyError: A name is not a known set.
    """
    seen = set()
    result: List[Pattern[str]] = []
    for name in names:
        if name not in PATTERN_SETS:
            raise KeyError(name)
        for rx in PATTERN_SETS[name]:
            if rx.pattern not in seen:
                seen.add(rx.pattern)
                result.append(rx)
    return result
