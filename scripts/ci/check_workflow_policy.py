#!/usr/bin/env python3
"""Workflow policy gate P1-P15 (milestone 0014, Story 02.0 Task 03; ADR 0005).

Checks the workflow files named on the command line (until the Story 03.0
cutover, only ``.github/workflows/ci-next.yml``) against the structural rules
of ``docs/roadmap/0014-ci-rework-from-scratch/02.0-pinned-toolchain-and-hardened-environment/03-workflow-lint-and-policy-gate.md``
(including its 2026-11-13 threat-model amendments, which override the earlier
text), plus the data files the rules read:

- ``scripts/ci/actions_allowlist.json`` (P13), ``scripts/ci/publish_jobs.json``
  and ``scripts/ci/publish_destinations.json`` (P8, P12),
  ``scripts/ci/step_budgets.json`` (P4) and ``scripts/ci/pins.json`` (P3);
- every file under ``.github/workflows/`` (P9);
- every file under ``scripts/ci/`` except ``reporters/``, ``env_names.py`` and
  ``tests/`` (P11; it scans scripts, not workflows).

Output follows RC1: one ``ok:`` line, or discrepancy lines
``<file>:<job>/<step>: <rule>: expected <x> -> actual <y>``, at most 20 of them
plus ``(+k more)``. Exit 0 on success, 1 on any violation, 2 for a usage error,
70 for an internal error.

**YAML.** Decided during implementation (recorded in the task's as-built
notes): workflows are read by :class:`_Loader`, a small stdlib-only loader for
the GitHub Actions subset (block mappings and sequences, plain/quoted scalars,
``|``/``>`` block scalars, one-line flow collections, comments). Anchors,
aliases, tags, merge keys, multi-line plain or quoted scalars and multiple
documents are rejected as a ``yaml`` violation (fail closed), never guessed.
actionlint's JSON output was not used: it does not expose comments, which P1
and P2 need, and it would make the gate depend on a downloaded binary.

**Guards.** ``if:`` expressions are evaluated, not pattern-matched: a small
evaluator of the GitHub expression subset runs each guard against a fixed set of
synthetic events (:data:`SCENARIOS`) with three-valued logic, and the set of
events where the guard is not provably false is compared with what the tier
allows. An expression the evaluator does not understand counts as reachable
from everywhere (fail closed).

The gate keeps honest code honest; it is not the security boundary (CR-1). The
boundary is the repository settings in ADR 0005 "Repository settings required".

Python 3.9+ standard library only; no shell; no network except the optional,
non-CI ``--verify-tags`` mode.
"""

from __future__ import annotations

import argparse
import datetime
import fnmatch
import json
import math
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, Iterator, List, Optional, Sequence, Set, Tuple

_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import get_reporter  # noqa: E402

REPO_ROOT = _HERE.parents[1]
LABEL = "workflow policy"

#: Platform expression delimiters, built by concatenation so that this script
#: does not itself contain the platform syntax P11 forbids.
EXPR_OPEN = "$" + "{{"
EXPR_CLOSE = "}}"
_EXPR = re.compile(re.escape(EXPR_OPEN) + r"(.*?)" + re.escape(EXPR_CLOSE), re.DOTALL)

#: RC1 bounds for the discrepancy output.
MAX_LINES = 20
LINE_WIDTH = 200
_FIELD_WIDTH = 80

#: Exit codes (ADR 0005 stage-command table).
EXIT_OK = 0
EXIT_VIOLATION = 1
EXIT_USAGE = 2
EXIT_INTERNAL = 70

#: P9: the only workflow file names allowed under ``.github/workflows/``.
#: (``ci-next.yml`` left this list at the Story 03.0 Task 04 cutover.)
ALLOWED_WORKFLOW_FILES: Tuple[str, ...] = ("ci.yml", "stage-*.yml")
_STAGE_FILE = re.compile(r"^stage-[a-z0-9][a-z0-9-]*\.yml$")
_LOCAL_STAGE_USES = re.compile(r"^\./\.github/workflows/stage-[a-z0-9][a-z0-9-]*\.yml$")
STAGE_PUBLISH_USES = "./.github/workflows/stage-publish.yml"

#: P8: the only triggers a workflow may name (CR-1).
ALLOWED_TRIGGERS: Tuple[str, ...] = ("push", "pull_request", "schedule", "workflow_dispatch",
                                     "workflow_call")
#: P8 / C22: platform debug switches (0011 R-6).
DEBUG_SWITCHES: Tuple[str, ...] = ("ACTIONS_STEP_DEBUG", "ACTIONS_RUNNER_DEBUG")

#: P7: the stage command, and the two bootstrap/lint entry points.
STAGE_SCRIPT = "scripts/ci/stage.py"
BOOTSTRAP_SCRIPTS: Tuple[str, ...] = ("scripts/ci/ci_step.py", "scripts/ci/lint.py")
#: Where the bootstrap scripts may be called directly: the lint job, the lint
#: stage workflow, and the entry workflow ``ci.yml`` (before ``stage.py``).
BOOTSTRAP_FILES: Tuple[str, ...] = ("ci.yml", "stage-lint.yml")
BOOTSTRAP_JOBS: Tuple[str, ...] = ("lint",)
_PYTHONS: Tuple[str, ...] = ("python3", "python")
#: P7: characters a run: line may contain. No quotes, ``$``, ``;``, ``&``,
#: ``|``, ``<``, ``>``, backticks, globs, braces or brackets.
_SAFE_RUN_LINE = re.compile(r"^[A-Za-z0-9_./=:+\- ]+$")

#: P10: documented tool variables a stage step may also receive.
TOOL_ENV_NAMES: Tuple[str, ...] = ("VCPKG_BINARY_SOURCES", "ASAN_OPTIONS", "UBSAN_OPTIONS")
#: P10 [OVERRIDES SPEC] context allowlist for the right-hand side of env: (CR-6).
ENV_CONTEXTS_EXACT: Tuple[str, ...] = (
    "github.run_id", "github.run_attempt", "github.server_url", "github.repository",
    "github.event_name", "github.ref", "github.ref_name", "github.ref_type", "github.sha",
    "github.workspace", "github.event.before", "github.event.pull_request.base.sha",
    "runner.temp")
_ENV_CONTEXT_PATTERNS = (re.compile(r"^matrix\.[A-Za-z0-9_-]+$"),
                         re.compile(r"^needs\.plan\.outputs\.[A-Za-z0-9_-]+$"))

#: P11 needles, built by concatenation (see :data:`EXPR_OPEN`).
P11_NEEDLES: Tuple[str, ...] = (EXPR_OPEN, "%" + "teamcity.", "##" + "teamcity[",
                                "::" + "error", "::" + "warning", "::" + "stop-commands")
_P11_ENV_READ = re.compile(r"\b(?:GITHUB|RUNNER|TEAMCITY)_[A-Z0-9_]*")
_P11_EMIT = re.compile(r"\bprint\s*\(|\bsys\.std(?:out|err)\.write\b")
P11_EXEMPT: Tuple[str, ...] = ("reporters", "env_names.py", "tests", "__pycache__")
#: Single emitter (CR-4): the only scripts/ci module that may write stdout/stderr.
P11_EMITTERS: Tuple[str, ...] = ("ci_step.py",)

#: P12: publish-capable permissions, and the permission classes of ADR 0005 D9.
PUBLISH_PERMISSIONS: Tuple[str, ...] = ("contents: write", "pull-requests: write",
                                        "id-token: write", "packages: write")
PERMISSION_CLASSES: Dict[str, Tuple[str, ...]] = {
    "repo-write": ("contents: write", "pull-requests: write"),
    "oidc": ("id-token: write",),
    "token-only": (),
}
TIERS: Tuple[str, ...] = ("release", "staging")
#: P12: the legacy ``-DGIT_TAG`` tag format (root CMakeLists.txt,
#: ``<version>.<major>.<minor>.<builddate><n>``) that no release trigger may match.
LEGACY_TAG_SAMPLES: Tuple[str, ...] = ("1.0.2.202611131", "1.2.3.4")

#: P13: actions whose job a neutral script can do, or that evaluate code.
DENIED_ACTIONS: Dict[str, str] = {
    "actions/github-script": "evaluates code from inputs",
    "lukka/run-vcpkg": "vcpkg bootstrap (bootstrap_vcpkg.py)",
    "lukka/get-cmake": "tool install (pins.json)",
    "actions/create-release": "release creation",
    "actions/upload-release-asset": "archive upload",
    "softprops/action-gh-release": "release creation",
    "ncipollo/release-action": "release creation",
    "svenstaro/upload-release-action": "archive upload",
    "marvinpinto/action-automatic-releases": "release creation",
    "jmgilman/actions-generate-checksum": "checksums",
    "thewh1teagle/checksum": "checksums",
}
ALLOWLIST_FIELDS: Tuple[str, ...] = ("action", "sha", "version", "justification",
                                     "teamcity_equivalent", "tag_commit_verified_on")
_SHA = re.compile(r"^[0-9a-f]{40}$")
_VERSION = re.compile(r"^v\d+\.\d+\.\d+$")
_VERSION_COMMENT = re.compile(r"^#\s*(v\d+\.\d+\.\d+)\s*$")
_ACTION_REF = re.compile(r"^([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_./-]+)?)@([^\s@]+)$")

#: P15
CACHE_ACTIONS: Tuple[str, ...] = ("actions/cache", "actions/cache/restore", "actions/cache/save")
CACHE_SAVING_ACTIONS: Tuple[str, ...] = ("actions/cache", "actions/cache/save")
NO_CACHE_FILES: Tuple[str, ...] = ("stage-verify.yml", "stage-publish.yml")
LOG_UPLOAD_FILE = "stage-build.yml"
#: Plan Q3 / 04.0 Task 01: every CI artifact is kept at most 3 days.
MAX_UPLOAD_RETENTION_DAYS = 3
_LITERAL_FILE = re.compile(r"^[A-Za-z0-9_.+-]+\.[A-Za-z0-9]+$")


# --------------------------------------------------------------------------
# Violations
# --------------------------------------------------------------------------

@dataclass(frozen=True)
class Violation:
    """One discrepancy, rendered as ``<file>:<location>: <rule>: expected <x> -> actual <y>``.

    Attributes:
        file: Display path of the offending file.
        location: ``workflow``, ``<job>``, ``<job>/<step>``, ``line <n>`` or a data-file key.
        rule: ``P1``..``P15``, or ``yaml`` for an unsupported/invalid workflow.
        expected: What the rule requires (short).
        actual: What was found (short, sanitised).
    """

    file: str
    location: str
    rule: str
    expected: str
    actual: str

    def render(self) -> str:
        """Return the RC1 discrepancy line, cut to the RC1 width."""
        line = (f"{self.file}:{self.location}: {self.rule}: expected {_short(self.expected)}"
                f" -> actual {_short(self.actual)}")
        return line[:LINE_WIDTH]


def _short(text: Any) -> str:
    """Return ``text`` on one line, without control characters, cut to the field width."""
    s = re.sub(r"[\x00-\x1f\x7f]", "?", str(text).replace("\n", "\\n"))
    return s if len(s) <= _FIELD_WIDTH else s[:_FIELD_WIDTH - 3] + "..."


def render_lines(violations: Sequence[Violation], limit: int = MAX_LINES) -> List[str]:
    """Return at most ``limit`` discrepancy lines plus one ``(+k more)`` line."""
    lines = [v.render() for v in violations[:limit]]
    if len(violations) > limit:
        lines.append(f"(+{len(violations) - limit} more)")
    return lines


def display_path(path: Path) -> str:
    """Return ``path`` relative to the repository root when inside it (POSIX separators)."""
    try:
        return path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


# --------------------------------------------------------------------------
# YAML subset loader
# --------------------------------------------------------------------------

class YamlSubsetError(ValueError):
    """The text is not valid YAML, or uses YAML outside the supported subset."""

    def __init__(self, line: int, message: str) -> None:
        super().__init__(f"line {line}: {message}")
        self.line = line
        self.message = message


class Scalar(str):
    """A scalar value with its source line, trailing comment and style.

    Every scalar stays a string (no implicit booleans or numbers): the rules
    compare the literal text, e.g. ``persist-credentials`` must be ``false``.
    """

    line: int
    comment: str
    style: str

    def __new__(cls, value: str, line: int = 0, comment: str = "",
                style: str = "plain") -> "Scalar":
        obj = super().__new__(cls, value)
        obj.line = line
        obj.comment = comment
        obj.style = style
        return obj


class YMap(dict):
    """A block or flow mapping that remembers each key's line and comments.

    ``key_comments[key]`` holds the comment-only lines directly above the key
    plus the trailing comment on the key's own line.
    """

    def __init__(self, line: int) -> None:
        super().__init__()
        self.line = line
        self.key_line: Dict[str, int] = {}
        self.key_comments: Dict[str, List[str]] = {}


class YList(list):
    """A block or flow sequence that remembers its first line."""

    def __init__(self, line: int) -> None:
        super().__init__()
        self.line = line


_PLAIN_KEY = re.compile(r"^([A-Za-z0-9_][A-Za-z0-9_.\-/]*)[ \t]*:(?=[ \t]|$)")
_QUOTED_KEY = re.compile(r"""^("(?:[^"\\]|\\.)*"|'(?:[^']|'')*')[ \t]*:(?=[ \t]|$)""")
_UNSUPPORTED_START = "&*!%@`"


def _is_item(text: str) -> bool:
    return text == "-" or text.startswith("- ")


class _Loader:
    """Recursive-descent loader for the GitHub Actions YAML subset (see module docstring)."""

    def __init__(self, text: str) -> None:
        self.raw = [ln.rstrip("\r") for ln in text.split("\n")]
        self.i = 0
        self.pending: List[str] = []
        self.vindent: Dict[int, int] = {}
        self.vtext: Dict[int, str] = {}

    def err(self, idx: int, message: str) -> YamlSubsetError:
        return YamlSubsetError(idx + 1, message)

    def indent_text(self, idx: int) -> Tuple[int, str]:
        if idx in self.vtext:
            return self.vindent[idx], self.vtext[idx]
        raw = self.raw[idx]
        stripped = raw.lstrip(" ")
        if stripped.startswith("\t"):
            raise self.err(idx, "tab in indentation")
        return len(raw) - len(stripped), stripped.rstrip(" \t")

    def peek(self) -> Optional[int]:
        """Skip blank and comment-only lines (collecting comments); return the next content line."""
        j = self.i
        while j < len(self.raw):
            _, text = self.indent_text(j)
            if not text:
                j += 1
                continue
            if text.startswith("#"):
                self.pending.append(text)
                j += 1
                continue
            self.i = j
            return j
        self.i = j
        return None

    def load(self) -> Any:
        j = self.peek()
        if j is None:
            return YMap(1)
        _, text = self.indent_text(j)
        if text.startswith("%"):
            raise self.err(j, "directives are not supported")
        if text.startswith("---"):
            if text != "---":
                raise self.err(j, "content on the document-start line is not supported")
            self.i = j + 1
            j = self.peek()
            if j is None:
                return YMap(1)
        node = self.block(self.indent_text(j)[0])
        j = self.peek()
        if j is not None:
            _, text = self.indent_text(j)
            if text == "...":
                self.i = j + 1
                j = self.peek()
            if j is not None:
                if self.indent_text(j)[1].startswith("---"):
                    raise self.err(j, "multiple documents are not supported")
                raise self.err(j, "unexpected content (bad indentation?)")
        return node

    def block(self, indent: int) -> Any:
        j = self.peek()
        assert j is not None
        _, text = self.indent_text(j)
        if _is_item(text):
            return self.seq(indent)
        if _PLAIN_KEY.match(text) or _QUOTED_KEY.match(text):
            return self.mapping(indent)
        self.i = j + 1
        value, comment = self.split_comment(text, j)
        node = self.inline(value, j, comment)
        self.no_continuation(indent)
        return node

    def mapping(self, indent: int) -> YMap:
        j0 = self.peek()
        assert j0 is not None
        out = YMap(j0 + 1)
        while True:
            j = self.peek()
            if j is None:
                break
            ind, text = self.indent_text(j)
            if ind < indent:
                break
            if ind > indent:
                raise self.err(j, "unexpected indentation")
            if _is_item(text):
                raise self.err(j, "sequence item where a mapping key was expected")
            m = _QUOTED_KEY.match(text) or _PLAIN_KEY.match(text)
            if m is None:
                raise self.err(j, "expected 'key: value'")
            key = m.group(1)
            if key[0] in "\"'":
                key = self.inline(key, j, "")
            key = str(key)
            if key in out:
                raise self.err(j, f"duplicate key {key!r}")
            comments, self.pending = self.pending, []
            self.i = j + 1
            value_text, trailing = self.split_comment(text[m.end():].strip(), j)
            out.key_line[key] = j + 1
            out.key_comments[key] = comments + ([trailing] if trailing else [])
            out[key] = self.value(value_text, trailing, indent, j)
        return out

    def value(self, value_text: str, trailing: str, indent: int, j: int) -> Any:
        if value_text == "":
            k = self.peek()
            if k is not None:
                kind, ktext = self.indent_text(k)
                if kind > indent or (kind == indent and _is_item(ktext)):
                    return self.block(kind)
            return Scalar("", j + 1, trailing)
        if value_text[0] in "|>":
            return self.block_scalar(value_text, indent, j)
        node = self.inline(value_text, j, trailing)
        self.no_continuation(indent)
        return node

    def no_continuation(self, indent: int) -> None:
        k = self.peek()
        if k is not None and self.indent_text(k)[0] > indent:
            raise self.err(k, "multi-line plain scalars are not supported")

    def seq(self, indent: int) -> YList:
        j0 = self.peek()
        assert j0 is not None
        out = YList(j0 + 1)
        while True:
            j = self.peek()
            if j is None:
                break
            ind, text = self.indent_text(j)
            if ind < indent:
                break
            if ind > indent:
                raise self.err(j, "unexpected indentation")
            if not _is_item(text):
                break
            rest = text[1:]
            stripped = rest.lstrip(" ")
            item_indent = ind + 1 + (len(rest) - len(stripped))
            if stripped == "" or stripped.startswith("#"):
                self.i = j + 1
                k = self.peek()
                if k is not None and self.indent_text(k)[0] > indent:
                    out.append(self.block(self.indent_text(k)[0]))
                else:
                    out.append(Scalar("", j + 1))
                continue
            if _is_item(stripped) or _PLAIN_KEY.match(stripped) or _QUOTED_KEY.match(stripped):
                self.vindent[j] = item_indent
                self.vtext[j] = stripped
                self.i = j
                out.append(self.block(item_indent))
                continue
            self.i = j + 1
            value_text, trailing = self.split_comment(stripped, j)
            if value_text[:1] in ("|", ">"):
                out.append(self.block_scalar(value_text, indent, j))
            else:
                out.append(self.inline(value_text, j, trailing))
                self.no_continuation(indent)
        return out

    def split_comment(self, value: str, j: int) -> Tuple[str, str]:
        """Split ``value`` into (value text, trailing comment)."""
        if not value:
            return "", ""
        if value.startswith("#"):
            return "", value
        if value[0] in "\"'":
            end = self.quoted_end(value, j)
        elif value[0] in "[{":
            end = self.flow_end(value, j)
        else:
            m = re.search(r"[ \t]#", value)
            if m:
                return value[:m.start()].rstrip(), value[m.start():].strip()
            return value.rstrip(), ""
        rest = value[end:]
        if rest.strip() == "":
            return value[:end], ""
        if rest[0] in " \t" and rest.strip().startswith("#"):
            return value[:end], rest.strip()
        raise self.err(j, "unexpected text after a quoted or flow value")

    def quoted_end(self, value: str, j: int) -> int:
        quote = value[0]
        k = 1
        while k < len(value):
            c = value[k]
            if quote == '"' and c == "\\":
                k += 2
                continue
            if c == quote:
                if quote == "'" and value[k + 1:k + 2] == "'":
                    k += 2
                    continue
                return k + 1
            k += 1
        raise self.err(j, "unterminated or multi-line quoted scalar")

    def flow_end(self, value: str, j: int) -> int:
        depth = 0
        k = 0
        while k < len(value):
            c = value[k]
            if c in "\"'":
                k += self.quoted_end(value[k:], j)
                continue
            if c in "[{":
                depth += 1
            elif c in "]}":
                depth -= 1
                if depth == 0:
                    return k + 1
            k += 1
        raise self.err(j, "unterminated or multi-line flow collection")

    def inline(self, value: str, j: int, comment: str) -> Any:
        c = value[0]
        if c == '"':
            try:
                text = json.loads(value)
            except ValueError:
                raise self.err(j, "unsupported escape in double-quoted scalar") from None
            return Scalar(text, j + 1, comment, "double")
        if c == "'":
            return Scalar(value[1:-1].replace("''", "'"), j + 1, comment, "single")
        if c in "[{":
            node, end = _FlowParser(value, j + 1, self).parse()
            if value[end:].strip():
                raise self.err(j, "unexpected text after a flow collection")
            return node
        if c in _UNSUPPORTED_START:
            raise self.err(j, "anchors, aliases, tags and reserved indicators are not supported")
        if ": " in value or value.endswith(":"):
            raise self.err(j, "ambiguous plain scalar containing ': '")
        return Scalar(value, j + 1, comment)

    def block_scalar(self, header: str, parent_indent: int, j: int) -> Scalar:
        m = re.fullmatch(r"([|>])([+-]?)", header)
        if m is None:
            raise self.err(j, "unsupported block scalar header (indentation indicators)")
        style, chomp = m.groups()
        lines: List[str] = []
        k = j + 1
        content_indent: Optional[int] = None
        while k < len(self.raw):
            raw = self.raw[k]
            if raw.strip() == "":
                lines.append("")
                k += 1
                continue
            ind = len(raw) - len(raw.lstrip(" "))
            if content_indent is None:
                if ind <= parent_indent:
                    break
                if raw.lstrip(" ").startswith("\t"):
                    raise self.err(k, "tab in indentation")
                content_indent = ind
            if ind < content_indent:
                break
            lines.append(raw[content_indent:])
            k += 1
        trailing_blank = 0
        while lines and lines[-1] == "":
            lines.pop()
            trailing_blank += 1
        self.i = k
        text = "\n".join(lines) if style == "|" else _fold(lines)
        if lines:
            if chomp == "+":
                text += "\n" * (1 + trailing_blank)
            elif chomp == "":
                text += "\n"
        return Scalar(text, j + 1, "", "literal" if style == "|" else "folded")


def _fold(lines: Sequence[str]) -> str:
    """Fold ``>`` block-scalar lines (simplified YAML folding)."""
    text = ""
    for idx, line in enumerate(lines):
        if idx == 0:
            text = line
            continue
        prev = lines[idx - 1]
        if line == "":
            text += "\n"
        elif prev == "":
            text += line
        elif line[:1] in (" ", "\t") or prev[:1] in (" ", "\t"):
            text += "\n" + line
        else:
            text += " " + line
    return text


class _FlowParser:
    """One-line flow collections: ``[a, 'b', {c: d}]``."""

    def __init__(self, text: str, line: int, loader: _Loader) -> None:
        self.s = text
        self.pos = 0
        self.line = line
        self.loader = loader

    def err(self, message: str) -> YamlSubsetError:
        return YamlSubsetError(self.line, message)

    def ws(self) -> None:
        while self.pos < len(self.s) and self.s[self.pos] in " \t":
            self.pos += 1

    def parse(self) -> Tuple[Any, int]:
        node = self.node(key=False)
        return node, self.pos

    def node(self, key: bool) -> Any:
        self.ws()
        if self.pos >= len(self.s):
            raise self.err("unterminated flow collection")
        c = self.s[self.pos]
        if c == "[":
            return self.seq()
        if c == "{":
            return self.mapping()
        if c in "\"'":
            end = self.pos + self.loader.quoted_end(self.s[self.pos:], self.line - 1)
            text = self.s[self.pos:end]
            self.pos = end
            return self.loader.inline(text, self.line - 1, "")
        start = self.pos
        stops = ",[]{}" + (":" if key else "")
        while self.pos < len(self.s) and self.s[self.pos] not in stops:
            self.pos += 1
        text = self.s[start:self.pos].strip()
        if text[:1] and text[0] in _UNSUPPORTED_START:
            raise self.err("anchors, aliases and tags are not supported")
        return Scalar(text, self.line)

    def seq(self) -> YList:
        out = YList(self.line)
        self.pos += 1
        self.ws()
        if self.s[self.pos:self.pos + 1] == "]":
            self.pos += 1
            return out
        while True:
            out.append(self.node(key=False))
            self.ws()
            c = self.s[self.pos:self.pos + 1]
            self.pos += 1
            if c == "]":
                return out
            if c != ",":
                raise self.err("expected ',' or ']' in a flow sequence")

    def mapping(self) -> YMap:
        out = YMap(self.line)
        self.pos += 1
        self.ws()
        if self.s[self.pos:self.pos + 1] == "}":
            self.pos += 1
            return out
        while True:
            key = str(self.node(key=True))
            self.ws()
            if self.s[self.pos:self.pos + 1] != ":":
                raise self.err("expected ':' in a flow mapping")
            self.pos += 1
            if key in out:
                raise self.err(f"duplicate key {key!r}")
            out.key_line[key] = self.line
            out.key_comments[key] = []
            out[key] = self.node(key=False)
            self.ws()
            c = self.s[self.pos:self.pos + 1]
            self.pos += 1
            if c == "}":
                return out
            if c != ",":
                raise self.err("expected ',' or '}' in a flow mapping")


def load_yaml(text: str) -> Any:
    """Load ``text`` with the subset loader.

    Raises:
        YamlSubsetError: Invalid YAML or YAML outside the supported subset.
    """
    return _Loader(text).load()


# --------------------------------------------------------------------------
# Expression evaluation (guards)
# --------------------------------------------------------------------------

class _Unknown:
    """The third truth value: the evaluator cannot decide."""

    def __repr__(self) -> str:
        return "UNKNOWN"


UNKNOWN = _Unknown()


def _scenario(event: str, ref: str, *, base_ref: str = "", head_ref: str = "") -> Dict[str, str]:
    ref_type = "tag" if ref.startswith("refs/tags/") else "branch"
    name = ref.split("/", 2)[2] if ref.count("/") >= 2 else ref
    return {"event_name": event, "ref": ref, "ref_type": ref_type, "ref_name": name,
            "base_ref": base_ref, "head_ref": head_ref}


#: Synthetic events every guard is evaluated against (P8, P12, P15).
SCENARIOS: Dict[str, Dict[str, str]] = {
    "push-master": _scenario("push", "refs/heads/master"),
    "push-branch": _scenario("push", "refs/heads/feature"),
    "push-tag-v": _scenario("push", "refs/tags/v1.2.3"),
    "push-tag-legacy": _scenario("push", "refs/tags/1.0.2.202611131"),
    "pull_request": _scenario("pull_request", "refs/pull/1/merge", base_ref="master",
                              head_ref="feature"),
    "pull_request-from-master": _scenario("pull_request", "refs/pull/2/merge",
                                          base_ref="master", head_ref="master"),
    "schedule": _scenario("schedule", "refs/heads/master"),
    "dispatch-master": _scenario("workflow_dispatch", "refs/heads/master"),
    "dispatch-branch": _scenario("workflow_dispatch", "refs/heads/feature"),
    "dispatch-tag-v": _scenario("workflow_dispatch", "refs/tags/v1.2.3"),
}
TIER_EVENTS: Dict[str, Set[str]] = {"release": {"push-tag-v"}, "staging": {"push-master"}}
TIER_GUARD_TEXT: Dict[str, str] = {"release": "v* tag push only",
                                   "staging": "master push only"}
CACHE_SAVE_EVENTS: Set[str] = {"push-master", "schedule"}

_TOKEN = re.compile(r"\s*(?:(?P<str>'(?:[^']|'')*')|(?P<num>\d+(?:\.\d+)?)"
                    r"|(?P<op>==|!=|<=|>=|&&|\|\||[!<>(),])"
                    r"|(?P<id>[A-Za-z_][A-Za-z0-9_-]*(?:\.[A-Za-z0-9_*-]+)*))")


class ExpressionError(ValueError):
    """The expression is outside the subset the evaluator understands."""


def _tokenize(text: str) -> List[Tuple[str, str]]:
    tokens: List[Tuple[str, str]] = []
    pos = 0
    while pos < len(text):
        if text[pos:].strip() == "":
            break
        m = _TOKEN.match(text, pos)
        if m is None or m.end() == pos:
            raise ExpressionError(f"unexpected {text[pos:pos + 10]!r}")
        kind = m.lastgroup or ""
        tokens.append((kind, m.group(kind)))
        pos = m.end()
    return tokens


def _truth(value: Any) -> Any:
    if value is UNKNOWN:
        return UNKNOWN
    if isinstance(value, str):
        return value != ""
    if value is None:
        return False
    if isinstance(value, bool):
        return value
    return value != 0


def _and(a: Any, b: Any) -> Any:
    ta, tb = _truth(a), _truth(b)
    if ta is False or tb is False:
        return False
    if ta is UNKNOWN or tb is UNKNOWN:
        return UNKNOWN
    return True


def _or(a: Any, b: Any) -> Any:
    ta, tb = _truth(a), _truth(b)
    if ta is True or tb is True:
        return True
    if ta is UNKNOWN or tb is UNKNOWN:
        return UNKNOWN
    return False


def _not(a: Any) -> Any:
    t = _truth(a)
    return UNKNOWN if t is UNKNOWN else not t


def _fold_case(v: Any) -> Any:
    return v.casefold() if isinstance(v, str) else v


class _Evaluator:
    """Recursive-descent evaluator of the GitHub expression subset used by guards."""

    def __init__(self, text: str, github: Dict[str, str]) -> None:
        self.tokens = _tokenize(text)
        self.pos = 0
        self.github = github

    def peek(self) -> Optional[str]:
        return self.tokens[self.pos][1] if self.pos < len(self.tokens) else None

    def take(self) -> Tuple[str, str]:
        if self.pos >= len(self.tokens):
            raise ExpressionError("unexpected end")
        tok = self.tokens[self.pos]
        self.pos += 1
        return tok

    def run(self) -> Any:
        value = self.or_()
        if self.pos != len(self.tokens):
            raise ExpressionError("trailing tokens")
        return value

    def or_(self) -> Any:
        value = self.and_()
        while self.peek() == "||":
            self.take()
            value = _or(value, self.and_())
        return value

    def and_(self) -> Any:
        value = self.not_()
        while self.peek() == "&&":
            self.take()
            value = _and(value, self.not_())
        return value

    def not_(self) -> Any:
        if self.peek() == "!":
            self.take()
            return _not(self.not_())
        return self.compare()

    def compare(self) -> Any:
        left = self.primary()
        op = self.peek()
        if op in ("==", "!=", "<", ">", "<=", ">="):
            self.take()
            right = self.primary()
            if left is UNKNOWN or right is UNKNOWN:
                return UNKNOWN
            a, b = _fold_case(left), _fold_case(right)
            if op == "==":
                return a == b
            if op == "!=":
                return a != b
            try:
                return {"<": a < b, ">": a > b, "<=": a <= b, ">=": a >= b}[op]
            except TypeError:
                return UNKNOWN
        return left

    def primary(self) -> Any:
        kind, text = self.take()
        if kind == "op" and text == "(":
            value = self.or_()
            if self.take()[1] != ")":
                raise ExpressionError("expected ')'")
            return value
        if kind == "str":
            return text[1:-1].replace("''", "'")
        if kind == "num":
            return float(text)
        if kind != "id":
            raise ExpressionError(f"unexpected {text!r}")
        if self.peek() == "(":
            self.take()
            args: List[Any] = []
            if self.peek() != ")":
                args.append(self.or_())
                while self.peek() == ",":
                    self.take()
                    args.append(self.or_())
            if self.take()[1] != ")":
                raise ExpressionError("expected ')'")
            return self.call(text, args)
        if text in ("true", "false"):
            return text == "true"
        if text == "null":
            return None
        return self.lookup(text)

    def call(self, name: str, args: List[Any]) -> Any:
        lname = name.lower()
        if lname in ("success", "always"):
            return True
        if lname in ("failure", "cancelled"):
            return False
        if lname in ("startswith", "endswith", "contains") and len(args) == 2:
            if UNKNOWN in args or not all(isinstance(a, str) for a in args):
                return UNKNOWN
            a, b = args[0].casefold(), args[1].casefold()
            return {"startswith": a.startswith(b), "endswith": a.endswith(b),
                    "contains": b in a}[lname]
        return UNKNOWN

    def lookup(self, ref: str) -> Any:
        parts = ref.split(".")
        if parts[0] == "github" and len(parts) == 2 and parts[1] in self.github:
            return self.github[parts[1]]
        if parts[0] == "needs" and len(parts) == 3 and parts[2] == "result":
            return "success"
        return UNKNOWN


def _guard_text(cond: Any) -> Optional[str]:
    """Return the bare expression of an ``if:`` value; ``None`` when it cannot be read."""
    s = str(cond).strip()
    if s.startswith(EXPR_OPEN) and s.endswith(EXPR_CLOSE) and s.count(EXPR_OPEN) == 1:
        return s[len(EXPR_OPEN):-len(EXPR_CLOSE)].strip()
    if EXPR_OPEN in s:
        return None  # mixed text: always truthy on the platform
    return s


def reachable(conditions: Iterable[Any]) -> Set[str]:
    """Return the :data:`SCENARIOS` in which all ``conditions`` are not provably false.

    ``None`` or empty conditions are always true. An expression the evaluator
    does not understand is unknown, so it never narrows the set (fail closed).
    """
    exprs: List[Optional[str]] = []
    for cond in conditions:
        if cond is None or str(cond).strip() == "":
            continue
        exprs.append(_guard_text(cond))
    out: Set[str] = set()
    for name, github in SCENARIOS.items():
        value: Any = True
        for expr in exprs:
            if expr is None:
                result: Any = UNKNOWN
            else:
                try:
                    result = _truth(_Evaluator(expr, github).run())
                except ExpressionError:
                    result = UNKNOWN
            value = _and(value, result)
        if value is not False:
            out.add(name)
    return out


def _reach_text(events: Set[str]) -> str:
    """Return ``reachable from a,b (+n)`` for a set of scenario names (bounded)."""
    names = sorted(events)
    more = f" (+{len(names) - 2})" if len(names) > 2 else ""
    return "reachable from " + ",".join(names[:2]) + more


def _context_refs(expr: str) -> List[str]:
    """Return the context references in ``expr`` (string literals and function names skipped)."""
    text = re.sub(r"'(?:[^']|'')*'", "''", expr)
    refs: List[str] = []
    for m in re.finditer(r"[A-Za-z_][A-Za-z0-9_-]*(?:\.[A-Za-z0-9_*-]+|\[[^\]]*\])*", text):
        token = m.group(0)
        if text[m.end():].lstrip().startswith("("):
            continue
        if token in ("true", "false", "null"):
            continue
        refs.append(token)
    return refs


# --------------------------------------------------------------------------
# Policy data
# --------------------------------------------------------------------------

@dataclass
class Policy:
    """Every data file the rules read, loaded once.

    Attributes:
        allowlist: P13 entries keyed by action.
        publish_jobs: P8/P12 publish-stage jobs (``file``, ``job``, ``tier``,
            ``permission_class``, ``permissions``).
        callers: P12 caller jobs of ``stage-publish.yml``.
        destinations: P12 registry entries.
        budgets: P4 data (``scripts/ci/step_budgets.json``).
        pins: P3 data (``scripts/ci/pins.json``).
        env_keys: P10 allowed ``env:`` keys for a stage step.
        destinations_dir: Where P12 looks for destination modules.
        paths: Display path of each data file, by role.
        errors: Violations found while loading the data files themselves.
    """

    allowlist: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    allowlist_entries: List[Any] = field(default_factory=list)
    publish_jobs: List[Dict[str, Any]] = field(default_factory=list)
    callers: List[Dict[str, Any]] = field(default_factory=list)
    destinations: List[Any] = field(default_factory=list)
    budgets: Dict[str, Any] = field(default_factory=dict)
    pins: Dict[str, Any] = field(default_factory=dict)
    env_keys: Set[str] = field(default_factory=set)
    destinations_dir: Path = _HERE / "destinations"
    paths: Dict[str, str] = field(default_factory=dict)
    errors: List[Violation] = field(default_factory=list)

    def publish_job(self, file: str, job: str) -> Optional[Dict[str, Any]]:
        return next((e for e in self.publish_jobs
                     if e.get("file") == file and e.get("job") == job), None)

    def caller(self, file: str, job: str) -> Optional[Dict[str, Any]]:
        return next((e for e in self.callers if e.get("file") == file and e.get("job") == job),
                    None)

    def job_budget(self, job: str) -> int:
        jobs = self.budgets.get("jobs", {})
        entry = jobs.get(job) if isinstance(jobs, dict) else None
        if isinstance(entry, dict) and isinstance(entry.get("budget_minutes"), int):
            return entry["budget_minutes"]
        default = self.budgets.get("default_job_minutes")
        return default if isinstance(default, int) else 10

    def step_budget(self, job: str, step: str) -> Optional[int]:
        steps = self.budgets.get("steps", {})
        value = steps.get(f"{job}/{step}") if isinstance(steps, dict) else None
        return value if isinstance(value, int) else None


@dataclass(frozen=True)
class PolicyFiles:
    """Paths of the policy data files (defaults are the repository's own)."""

    actions_allowlist: Path = _HERE / "actions_allowlist.json"
    publish_jobs: Path = _HERE / "publish_jobs.json"
    publish_destinations: Path = _HERE / "publish_destinations.json"
    step_budgets: Path = _HERE / "step_budgets.json"
    pins: Path = _HERE / "pins.json"
    destinations_dir: Path = _HERE / "destinations"


def _load_json(path: Path, rule: str, errors: List[Violation]) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        errors.append(Violation(display_path(path), "file", rule, "data file present", "missing"))
    except (OSError, ValueError) as exc:
        errors.append(Violation(display_path(path), "file", rule, "valid JSON",
                                type(exc).__name__))
    return None


def load_policy(files: PolicyFiles = PolicyFiles()) -> Policy:
    """Load every policy data file; problems become :attr:`Policy.errors`, never exceptions."""
    errors: List[Violation] = []
    policy = Policy(destinations_dir=files.destinations_dir)
    policy.paths = {"allowlist": display_path(files.actions_allowlist),
                    "publish_jobs": display_path(files.publish_jobs),
                    "destinations": display_path(files.publish_destinations),
                    "budgets": display_path(files.step_budgets),
                    "pins": display_path(files.pins)}

    data = _load_json(files.actions_allowlist, "P13", errors)
    if data is not None:
        entries = data.get("actions") if isinstance(data, dict) else None
        if not isinstance(entries, list):
            errors.append(Violation(policy.paths["allowlist"], "actions", "P13",
                                    "an 'actions' list", type(entries).__name__))
        else:
            policy.allowlist_entries = entries
            for entry in entries:
                if isinstance(entry, dict) and isinstance(entry.get("action"), str):
                    policy.allowlist[entry["action"]] = entry

    data = _load_json(files.publish_jobs, "P12", errors)
    if data is not None:
        for key in ("jobs", "callers"):
            value = data.get(key) if isinstance(data, dict) else None
            if not isinstance(value, list) or not all(isinstance(e, dict) for e in value):
                errors.append(Violation(policy.paths["publish_jobs"], key, "P12",
                                        f"a '{key}' list of objects", type(value).__name__))
                value = []
            setattr(policy, "publish_jobs" if key == "jobs" else "callers", value)

    data = _load_json(files.publish_destinations, "P12", errors)
    if data is not None:
        value = data.get("destinations") if isinstance(data, dict) else None
        if not isinstance(value, list):
            errors.append(Violation(policy.paths["destinations"], "destinations", "P12",
                                    "a 'destinations' list", type(value).__name__))
        else:
            policy.destinations = value

    data = _load_json(files.step_budgets, "P4", errors)
    if isinstance(data, dict):
        policy.budgets = data
    data = _load_json(files.pins, "P3", errors)
    if isinstance(data, dict):
        policy.pins = data

    credentials: Set[str] = set()
    for entry in policy.destinations:
        if isinstance(entry, dict) and isinstance(entry.get("credentials"), list):
            credentials.update(c for c in entry["credentials"] if isinstance(c, str))
    policy.env_keys = set(env_names.NAMES) | set(TOOL_ENV_NAMES) | credentials
    policy.errors = errors
    return policy


# --------------------------------------------------------------------------
# Workflow helpers
# --------------------------------------------------------------------------

def _items(node: Any) -> List[Any]:
    if isinstance(node, list):
        return list(node)
    if isinstance(node, str) and node != "":
        return [node]
    return []


def _triggers(root: YMap) -> Dict[str, Any]:
    on = root.get("on", root.get("true"))
    if isinstance(on, YMap):
        return dict(on)
    return {str(t): None for t in _items(on)}


def _expressions(text: str) -> List[str]:
    return [m.group(1) for m in _EXPR.finditer(text)]


def _walk_scalars(node: Any) -> Iterator[str]:
    if isinstance(node, dict):
        for value in node.values():
            yield from _walk_scalars(value)
    elif isinstance(node, list):
        for value in node:
            yield from _walk_scalars(value)
    elif isinstance(node, str):
        yield node


def _uses_secrets(node: Any) -> bool:
    return any(re.search(r"\bsecrets\b", expr)
               for s in _walk_scalars(node) for expr in _expressions(s))


def _granted(perms: Any) -> Set[str]:
    """Return the ``scope: level`` grants of a ``permissions:`` value (``none`` dropped)."""
    if isinstance(perms, dict):
        return {f"{k}: {v}" for k, v in perms.items() if str(v) != "none"}
    if isinstance(perms, str):
        if perms == "write-all":
            return set(PUBLISH_PERMISSIONS) | {"write-all"}
        if perms:
            return {perms}
    return set()


def _render_perms(perms: Any) -> str:
    if perms is None:
        return "none"
    if isinstance(perms, dict):
        return "{" + ", ".join(sorted(_granted(perms))) + "}" if perms else "{}"
    return str(perms)


def _environment(job: YMap) -> str:
    env = job.get("environment")
    if isinstance(env, dict):
        env = env.get("name")
    return str(env) if env is not None else ""


def _needs(job: YMap) -> List[str]:
    return [str(n) for n in _items(job.get("needs"))]


def _step_label(step: Any, index: int) -> str:
    label = ""
    if isinstance(step, dict):
        label = str(step.get("id") or step.get("name") or "")
    label = re.sub(r"[^A-Za-z0-9 ._()+-]", "?", label)[:40]
    return label or f"step{index + 1}"


def _action_name(uses: str) -> str:
    return uses.split("@", 1)[0].strip()


def _int(node: Any) -> Optional[int]:
    if isinstance(node, str) and re.fullmatch(r"[0-9]+", node.strip()):
        return int(node)
    return None


def _glob_regex(pattern: str) -> "re.Pattern[str]":
    """Translate a GitHub filter pattern (``*``, ``**``, ``?``, ``+``, ``[..]``) to a regex."""
    out = ""
    k = 0
    while k < len(pattern):
        c = pattern[k]
        if c == "*":
            if pattern[k:k + 2] == "**":
                out += ".*"
                k += 2
                continue
            out += "[^/]*"
        elif c in "?+" and out:
            out += c
        elif c == "[":
            end = pattern.find("]", k)
            if end < 0:
                out += re.escape(c)
            else:
                out += pattern[k:end + 1]
                k = end + 1
                continue
        else:
            out += re.escape(c)
        k += 1
    return re.compile(out + r"\Z")


def run_blocks(path: Path) -> List[str]:
    """Return every ``run:`` body in the workflow at ``path`` (for test_shell_flags.py).

    Raises:
        YamlSubsetError: The workflow is not in the supported YAML subset.
    """
    root = load_yaml(path.read_text(encoding="utf-8"))
    jobs = root.get("jobs") if isinstance(root, dict) else None
    out: List[str] = []
    for job in (jobs or {}).values():
        if isinstance(job, dict):
            for step in _items(job.get("steps")):
                if isinstance(step, dict) and "run" in step:
                    out.append(str(step["run"]))
    return out


# --------------------------------------------------------------------------
# Per-workflow rules
# --------------------------------------------------------------------------

class _WorkflowCheck:
    """Applies the workflow rules (P1-P10, P12-P15) to one parsed workflow."""

    def __init__(self, path: Path, root: YMap, text: str, policy: Policy) -> None:
        self.path = path
        self.name = path.name
        self.file = display_path(path)
        self.root = root
        self.text = text
        self.policy = policy
        self.out: List[Violation] = []
        self.triggers = _triggers(root)
        self.call_only = set(self.triggers) == {"workflow_call"}
        jobs = root.get("jobs")
        self.jobs: Dict[str, YMap] = ({str(k): v for k, v in jobs.items() if isinstance(v, dict)}
                                      if isinstance(jobs, dict) else {})

    def v(self, location: str, rule: str, expected: str, actual: Any) -> None:
        self.out.append(Violation(self.file, location, rule, expected, str(actual)))

    def steps(self, job: YMap) -> List[Tuple[int, Any]]:
        return list(enumerate(_items(job.get("steps"))))

    def run(self) -> List[Violation]:
        if not self.jobs:
            self.v("workflow", "yaml", "a 'jobs' mapping", "none")
        self.p1()
        self.p8_workflow()
        self.p9_triggers()
        self.p10_env()
        self.p12_tags()
        for job_id, job in self.jobs.items():
            self.p1_job(job_id, job)
            self.p3(job_id, job)
            self.p4(job_id, job)
            self.p5(job_id, job)
            self.p8_job(job_id, job)
            self.p12_job(job_id, job)
            if "uses" in job:
                self.uses(job_id, job["uses"], job_level=True)
            for index, step in self.steps(job):
                loc = f"{job_id}/{_step_label(step, index)}"
                if not isinstance(step, dict):
                    self.v(loc, "yaml", "a step mapping", type(step).__name__)
                    continue
                if "run" in step:
                    self.p6_p7(job_id, loc, step)
                if "uses" in step:
                    self.uses(loc, step["uses"], job_level=False)
                    self.p14(loc, step)
                    self.p15(job_id, job, loc, step)
        return self.out

    # P1 ---------------------------------------------------------------
    def p1(self) -> None:
        perms = self.root.get("permissions")
        if not (isinstance(perms, dict) and _granted(perms) == {"contents: read"}
                and len(perms) == 1):
            self.v("workflow", "P1", "permissions: contents: read", _render_perms(perms))

    def p1_job(self, job_id: str, job: YMap) -> None:
        if "permissions" not in job:
            return
        if not (_granted(job["permissions"]) - {"contents: read"}):
            return
        comments = job.key_comments.get("permissions", [])
        if not any(re.match(r"^#\s*permissions:\s*\S", c) for c in comments):
            self.v(job_id, "P1", "'# permissions: <reason>' comment on a widening",
                   _render_perms(job["permissions"]))

    # P2 / P13 ------------------------------------------------------------
    def uses(self, loc: str, node: Any, job_level: bool) -> None:
        ref = str(node).strip()
        if ref.startswith("docker://"):
            self.v(loc, "P2", "owner/repo@<40-hex sha>", "docker:// reference")
            return
        if ref.startswith("./"):
            if job_level and _LOCAL_STAGE_USES.match(ref):
                return
            self.v(loc, "P13", "no local or composite action", ref)
            return
        m = _ACTION_REF.match(ref)
        if m is None:
            self.v(loc, "P2", "owner/repo@<40-hex sha>", ref)
            return
        action, rev = m.groups()
        comment = getattr(node, "comment", "") or ""
        version = _VERSION_COMMENT.match(comment)
        pinned = bool(_SHA.match(rev))
        if not pinned:
            self.v(loc, "P2", "@<40-hex sha>", f"@{rev}")
        if version is None:
            self.v(loc, "P2", "'# vX.Y.Z' comment", comment or "none")
        repo = "/".join(action.lower().split("/")[:2])
        if repo in DENIED_ACTIONS:
            self.v(loc, "P13", f"no action a neutral script can do ({DENIED_ACTIONS[repo]})",
                   action)
            return
        entry = self.policy.allowlist.get(action)
        if entry is None:
            self.v(loc, "P13", "action listed in actions_allowlist.json", action)
            return
        if pinned and rev != entry.get("sha"):
            self.v(loc, "P13", f"allowlisted sha {str(entry.get('sha'))[:12]}", rev[:12])
        if version is not None and version.group(1) != entry.get("version"):
            self.v(loc, "P13", f"allowlisted version {entry.get('version')}", version.group(1))

    # P3 -----------------------------------------------------------------
    def p3(self, job_id: str, job: YMap) -> None:
        matrix = job.get("strategy", {}).get("matrix") if isinstance(job.get("strategy"), dict) \
            else None

        def matrix_values(key: str) -> List[str]:
            values: List[str] = []
            if isinstance(matrix, dict):
                values += [str(x) for x in _items(matrix.get(key))]
                for inc in _items(matrix.get("include")):
                    if isinstance(inc, dict) and key in inc:
                        values += [str(x) for x in _items(inc[key])]
            return values

        labels: List[str] = []
        runs_on = job.get("runs-on")
        candidates = _items(runs_on.get("labels")) if isinstance(runs_on, dict) else _items(runs_on)
        for value in candidates:
            value = str(value)
            if EXPR_OPEN in value:
                for key in re.findall(r"matrix\.([A-Za-z0-9_-]+)", value):
                    labels += matrix_values(key)
            else:
                labels.append(value)
        labels += matrix_values("os")
        seen: Set[str] = set()
        for label in labels:
            if label.strip().endswith("-latest") and label not in seen:
                seen.add(label)
                self.v(job_id, "P3", "pinned runner label", label)

    # P4 -----------------------------------------------------------------
    def p4(self, job_id: str, job: YMap) -> None:
        budget = self.policy.job_budget(job_id)
        cap = budget
        if "uses" not in job:  # a reusable-workflow caller cannot carry timeout-minutes
            value = job.get("timeout-minutes")
            minutes = _int(value)
            if value is None:
                self.v(job_id, "P4", f"timeout-minutes <= {budget}", "none")
            elif minutes is None or not 1 <= minutes <= budget:
                self.v(job_id, "P4", f"timeout-minutes <= {budget}", value)
            else:
                cap = minutes
        for index, step in self.steps(job):
            if not isinstance(step, dict) or not ("run" in step or "uses" in step):
                continue
            label = _step_label(step, index)
            step_cap = min(cap, self.policy.step_budget(job_id, label) or cap)
            value = step.get("timeout-minutes")
            minutes = _int(value)
            if value is None:
                self.v(f"{job_id}/{label}", "P4", f"timeout-minutes <= {step_cap}", "none")
            elif minutes is None or not 1 <= minutes <= step_cap:
                self.v(f"{job_id}/{label}", "P4", f"timeout-minutes <= {step_cap}", value)

    # P5 -----------------------------------------------------------------
    def p5(self, job_id: str, job: YMap) -> None:
        targets = [(job_id, job)] + [(f"{job_id}/{_step_label(s, i)}", s)
                                     for i, s in self.steps(job) if isinstance(s, dict)]
        for loc, node in targets:
            if "continue-on-error" in node and str(node["continue-on-error"]) != "false":
                self.v(loc, "P5", "no continue-on-error (ci_step --best-effort)",
                       f"continue-on-error: {node['continue-on-error']}")

    # P6 / P7 ------------------------------------------------------------
    def p6_p7(self, job_id: str, loc: str, step: YMap) -> None:
        body = str(step["run"])
        if EXPR_OPEN in body:
            self.v(loc, "P6", "no expression in run:", "expression in run: body")
        shell = step.get("shell")
        if shell is not None and str(shell) != "bash":
            self.v(loc, "P7", "shell unset or bash", shell)
        lines = [ln for ln in _EXPR.sub("X", body).split("\n") if ln.strip()]
        if len(lines) != 1:
            self.v(loc, "P7", "one-line run:", f"{len(lines)} lines")
            return
        line = lines[0].strip()
        bad = next((c for c in line if not _SAFE_RUN_LINE.match(c)), None)
        if bad is not None:
            self.v(loc, "P7", "no shell control flow, quoting or redirection", repr(bad))
            return
        words = line.split()
        allowed = {STAGE_SCRIPT}
        if self.name in BOOTSTRAP_FILES or job_id in BOOTSTRAP_JOBS:
            allowed.update(BOOTSTRAP_SCRIPTS)
        if len(words) < 2 or words[0] not in _PYTHONS or words[1] not in allowed:
            self.v(loc, "P7", f"python3 {STAGE_SCRIPT} ...", " ".join(words[:2]))

    # P8 -----------------------------------------------------------------
    def p8_workflow(self) -> None:
        for name in self.triggers:
            if name not in ALLOWED_TRIGGERS:
                self.v("workflow", "P8", "trigger in " + "/".join(ALLOWED_TRIGGERS), name)
        for number, line in enumerate(self.text.split("\n"), 1):
            for switch in DEBUG_SWITCHES:
                if switch in line:
                    self.v(f"line {number}", "P8", "no platform debug switch", switch)
        top = {k: v for k, v in self.root.items() if k != "jobs"}
        if _uses_secrets(top):
            self.v("workflow", "P8", "no secrets outside a publish job", "secrets reference")

    def p8_job(self, job_id: str, job: YMap) -> None:
        if str(job.get("secrets", "")) == "inherit":
            self.v(job_id, "P8", "secrets passed by name", "secrets: inherit")
        credentials = []
        if _uses_secrets(job):
            credentials.append("secrets reference")
        if "id-token: write" in _granted(job.get("permissions")):
            credentials.append("id-token: write")
        entry = self.policy.publish_job(self.name, job_id)
        if str(job.get("uses", "")).strip() == STAGE_PUBLISH_USES:
            if credentials and self.policy.caller(self.name, job_id) is None:
                self.v(job_id, "P8", "credentials only in a listed publish caller",
                       ", ".join(credentials))
            return  # tier clause of a caller: P12
        if entry is None or self.name != "stage-publish.yml":
            if credentials:
                self.v(job_id, "P8", "credentials only in a listed stage-publish.yml job",
                       ", ".join(credentials))
            return
        tier = str(entry.get("tier"))
        if _environment(job) != tier:
            self.v(job_id, "P8", f"environment: {tier}", _environment(job) or "none")
        if not self.call_only and "verify" not in _needs(job):
            self.v(job_id, "P8", "needs: verify", ",".join(_needs(job)) or "none")
        self.tier_guard(job_id, "P8", tier, [job.get("if")])

    def tier_guard(self, loc: str, rule: str, tier: str, conditions: List[Any]) -> None:
        allowed = TIER_EVENTS.get(tier)
        if allowed is None:
            self.v(loc, rule, "tier release or staging", tier)
            return
        extra = reachable(conditions) - allowed
        if extra:
            self.v(loc, rule, f"{tier} guard: {TIER_GUARD_TEXT[tier]}",
                   _reach_text(extra))

    # P9 -----------------------------------------------------------------
    def p9_triggers(self) -> None:
        if _STAGE_FILE.match(self.name) and set(self.triggers) != {"workflow_call"}:
            self.v("workflow", "P9", "on: workflow_call only (stage workflow)",
                   ",".join(self.triggers) or "none")

    # P10 ----------------------------------------------------------------
    def p10_env(self) -> None:
        choice_inputs: Set[str] = set()
        for trigger in ("workflow_dispatch", "workflow_call"):
            spec = self.triggers.get(trigger)
            inputs = spec.get("inputs") if isinstance(spec, dict) else None
            if isinstance(inputs, dict):
                choice_inputs.update(str(k) for k, v in inputs.items()
                                     if isinstance(v, dict) and str(v.get("type")) == "choice")

        def allowed(ref: str) -> bool:
            if ref in ENV_CONTEXTS_EXACT or any(p.match(ref) for p in _ENV_CONTEXT_PATTERNS):
                return True
            m = re.fullmatch(r"inputs\.([A-Za-z0-9_-]+)", ref)
            return bool(m and m.group(1) in choice_inputs)

        envs: List[Tuple[str, Any]] = [("workflow", self.root.get("env"))]
        for job_id, job in self.jobs.items():
            envs.append((job_id, job.get("env")))
            for index, step in self.steps(job):
                if isinstance(step, dict):
                    envs.append((f"{job_id}/{_step_label(step, index)}", step.get("env")))
        for loc, env in envs:
            if not isinstance(env, dict):
                continue
            for key, value in env.items():
                for expr in _expressions(str(value)):
                    for ref in _context_refs(expr):
                        if ref.split(".")[0].split("[")[0] == "secrets":
                            continue  # credentials are P8's
                        if not allowed(ref):
                            self.v(loc, "P10", "allowlisted context in env:", f"{key}: {ref}")

        stage_jobs = [(job_id, job) for job_id, job in self.jobs.items()
                      if any(isinstance(s, dict) and STAGE_SCRIPT in str(s.get("run", ""))
                             for _, s in self.steps(job))]
        if not stage_jobs:
            return
        scopes: List[Tuple[str, Any]] = [("workflow", self.root.get("env"))]
        for job_id, job in stage_jobs:
            scopes.append((job_id, job.get("env")))
            for index, step in self.steps(job):
                if isinstance(step, dict) and STAGE_SCRIPT in str(step.get("run", "")):
                    scopes.append((f"{job_id}/{_step_label(step, index)}", step.get("env")))
        for loc, env in scopes:
            if isinstance(env, dict):
                for key in env:
                    if key not in self.policy.env_keys:
                        self.v(loc, "P10", "neutral CI_* or documented tool name", key)

    # P12 ----------------------------------------------------------------
    def p12_tags(self) -> None:
        push = self.triggers.get("push")
        if not isinstance(push, dict):
            return
        for pattern in _items(push.get("tags")):
            pattern = str(pattern)
            if pattern.startswith("!"):
                continue
            rx = _glob_regex(pattern)
            hit = next((s for s in LEGACY_TAG_SAMPLES if rx.match(s)), None)
            if hit is not None:
                self.v("workflow", "P12", "tag pattern not matching legacy GIT_TAG tags",
                       f"{pattern} matches {hit}")

    def p12_job(self, job_id: str, job: YMap) -> None:
        granted = _granted(job.get("permissions"))
        publish = {p for p in granted if p in PUBLISH_PERMISSIONS or p == "write-all"}
        is_caller = str(job.get("uses", "")).strip() == STAGE_PUBLISH_USES
        entry = (self.policy.caller(self.name, job_id) if is_caller
                 else self.policy.publish_job(self.name, job_id))
        if entry is None:
            if is_caller:
                self.v(job_id, "P12", "stage-publish.yml caller listed in publish_jobs.json",
                       "unlisted")
            elif publish:
                self.v(job_id, "P12", "publish permission only on a listed publish job",
                       ", ".join(sorted(publish)))
            return
        declared = {str(p) for p in _items(entry.get("permissions"))} | {"contents: read"}
        extra = granted - declared
        if extra:
            self.v(job_id, "P12", "permissions within publish_jobs.json entry",
                   ", ".join(sorted(extra)))
        if is_caller:
            if "verify" not in _needs(job):
                self.v(job_id, "P12", "caller needs: verify", ",".join(_needs(job)) or "none")
            self.tier_guard(job_id, "P12", str(entry.get("tier")), [job.get("if")])

    # P14 ----------------------------------------------------------------
    def p14(self, loc: str, step: YMap) -> None:
        if _action_name(str(step["uses"])) != "actions/checkout":
            return
        with_ = step.get("with")
        value = with_.get("persist-credentials") if isinstance(with_, dict) else None
        if str(value) != "false" or value is None:
            self.v(loc, "P14", "persist-credentials: false", value if value is not None else "unset")

    # P15 ----------------------------------------------------------------
    def p15(self, job_id: str, job: YMap, loc: str, step: YMap) -> None:
        action = _action_name(str(step["uses"]))
        if action in CACHE_ACTIONS:
            if self.name in NO_CACHE_FILES:
                self.v(loc, "P15", f"no cache in {self.name}", action)
                return
            if action in CACHE_SAVING_ACTIONS:
                extra = reachable([job.get("if"), step.get("if")]) - CACHE_SAVE_EVENTS
                if extra:
                    self.v(loc, "P15", "cache save only on master push or schedule",
                           _reach_text(extra))
            return
        if action != "actions/upload-artifact":
            return
        # 2026-11-13 Story 03.0 review S3-1: every upload-artifact step is
        # checked, whatever its artifact name; only the logs-* file placement
        # is keyed on the name.
        with_ = step.get("with") if isinstance(step.get("with"), dict) else {}
        if str(with_.get("name", "")).startswith("logs-") and self.name != LOG_UPLOAD_FILE:
            self.v(loc, "P15", f"logs-* upload only in {LOG_UPLOAD_FILE}", self.name)
        paths = [p.strip() for p in str(with_.get("path", "")).split("\n") if p.strip()]
        if not paths:
            self.v(loc, "P15", "literal file paths", "no path")
        for p in paths:
            plain = _EXPR.sub("X", p).rstrip()
            last = plain.split("/")[-1]
            if any(c in p for c in "*?!") or p.endswith("/") or not _LITERAL_FILE.match(last):
                self.v(loc, "P15", "literal file path (no directory or glob)", p)
            elif any(part.casefold() == ".git" for part in re.split(r"[/\\]", plain)):
                self.v(loc, "P15", "no .git path component", p)
        if str(with_.get("include-hidden-files", "")) != "false":
            self.v(loc, "P15", "include-hidden-files: false",
                   with_.get("include-hidden-files", "unset"))
        retention = str(with_.get("retention-days", "")).strip()
        if not (retention.isdigit() and 1 <= int(retention) <= MAX_UPLOAD_RETENTION_DAYS):
            self.v(loc, "P15", f"retention-days 1..{MAX_UPLOAD_RETENTION_DAYS}",
                   retention or "unset")


def check_workflow(path: Path, allowed_files: Sequence[str] = ALLOWED_WORKFLOW_FILES,
                   policy: Optional[Policy] = None) -> List[Violation]:
    """Return every violation in the workflow file at ``path``.

    Args:
        path: The workflow file.
        allowed_files: P9 file-name patterns (``fnmatch``).
        policy: Loaded policy data; ``None`` loads the repository's own files.
    """
    policy = policy if policy is not None else load_policy()
    file = display_path(path)
    out: List[Violation] = []
    if not any(fnmatch.fnmatchcase(path.name, pat) for pat in allowed_files):
        out.append(Violation(file, "workflow", "P9", "file in " + ", ".join(allowed_files),
                             path.name))
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        return out + [Violation(file, "workflow", "yaml", "readable UTF-8 file",
                                type(exc).__name__)]
    try:
        root = load_yaml(text)
    except YamlSubsetError as exc:
        return out + [Violation(file, f"line {exc.line}", "yaml", "supported YAML subset",
                                exc.message)]
    if not isinstance(root, YMap):
        return out + [Violation(file, "workflow", "yaml", "a mapping", type(root).__name__)]
    return out + _WorkflowCheck(path, root, text, policy).run()


# --------------------------------------------------------------------------
# Directory, script and data-file rules
# --------------------------------------------------------------------------

def check_workflows_dir(directory: Path, checked: Sequence[Path],
                        allowed_files: Sequence[str] = ALLOWED_WORKFLOW_FILES) -> List[Violation]:
    """P9: every workflow file in ``directory`` not already checked has an allowed name.

    A stage file there must also be ``on: workflow_call`` only.
    """
    out: List[Violation] = []
    if not directory.is_dir():
        return out
    done = {p.resolve() for p in checked}
    for path in sorted(directory.iterdir()):
        if path.suffix not in (".yml", ".yaml") or path.resolve() in done:
            continue
        file = display_path(path)
        if not any(fnmatch.fnmatchcase(path.name, pat) for pat in allowed_files):
            out.append(Violation(file, "workflow", "P9", "file in " + ", ".join(allowed_files),
                                 path.name))
            continue
        if _STAGE_FILE.match(path.name):
            try:
                root = load_yaml(path.read_text(encoding="utf-8"))
            except (OSError, UnicodeDecodeError, YamlSubsetError):
                out.append(Violation(file, "workflow", "yaml", "supported YAML subset",
                                     "unreadable"))
                continue
            triggers = _triggers(root) if isinstance(root, YMap) else {}
            if set(triggers) != {"workflow_call"}:
                out.append(Violation(file, "workflow", "P9", "on: workflow_call only",
                                     ",".join(triggers) or "none"))
    return out


def check_scripts(scripts_dir: Path) -> List[Violation]:
    """P11: no platform syntax, no platform variable and a single emitter under ``scripts_dir``."""
    out: List[Violation] = []
    if not scripts_dir.is_dir():
        return out
    for path in sorted(scripts_dir.rglob("*")):
        rel = path.relative_to(scripts_dir)
        if not path.is_file() or rel.parts[0] in P11_EXEMPT or "__pycache__" in rel.parts \
                or path.suffix in (".pyc", ".pyo"):
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            out.append(Violation(display_path(path), "file", "P11", "readable file",
                                 type(exc).__name__))
            continue
        emitter = path.suffix == ".py" and rel.as_posix() not in P11_EMITTERS
        for number, line in enumerate(text.split("\n"), 1):
            hits = [n for n in P11_NEEDLES if n in line] + _P11_ENV_READ.findall(line)
            if emitter:
                hits += [m.group(0) for m in _P11_EMIT.finditer(line)]
            for hit in hits:
                out.append(Violation(display_path(path), f"line {number}", "P11",
                                     "no platform syntax, platform variable or direct output",
                                     hit))
    return out


def _iso_date(value: Any) -> bool:
    if not isinstance(value, str) or not re.fullmatch(r"\d{4}-\d{2}-\d{2}", value):
        return False
    try:
        datetime.date.fromisoformat(value)
    except ValueError:
        return False
    return True


def check_allowlist(policy: Policy) -> List[Violation]:
    """P13: every allowlist entry is complete, pinned, dated and not a denied action."""
    out: List[Violation] = []
    file = policy.paths.get("allowlist", "actions_allowlist.json")
    seen: Set[str] = set()
    for index, entry in enumerate(policy.allowlist_entries):
        if not isinstance(entry, dict):
            out.append(Violation(file, f"actions[{index}]", "P13", "an object",
                                 type(entry).__name__))
            continue
        loc = str(entry.get("action") or f"actions[{index}]")
        for key in ALLOWLIST_FIELDS:
            if not isinstance(entry.get(key), str) or not entry[key].strip():
                out.append(Violation(file, loc, "P13", f"field {key}", "missing"))
        action = str(entry.get("action", ""))
        if action in seen:
            out.append(Violation(file, loc, "P13", "one entry per action", "duplicate"))
        seen.add(action)
        if action and not re.fullmatch(r"[A-Za-z0-9_-][A-Za-z0-9_.-]*/[A-Za-z0-9_.-]+"
                                       r"(/[A-Za-z0-9_./-]+)?", action):
            out.append(Violation(file, loc, "P13", "owner/repo[/path] action", action))
        repo = "/".join(action.lower().split("/")[:2])
        if repo in DENIED_ACTIONS:
            out.append(Violation(file, loc, "P13",
                                 f"no action a neutral script can do ({DENIED_ACTIONS[repo]})",
                                 action))
        if isinstance(entry.get("sha"), str) and not _SHA.match(entry["sha"]):
            out.append(Violation(file, loc, "P13", "40-hex sha", entry["sha"]))
        if isinstance(entry.get("version"), str) and not _VERSION.match(entry["version"]):
            out.append(Violation(file, loc, "P13", "version vX.Y.Z", entry["version"]))
        if "tag_commit_verified_on" in entry and isinstance(entry["tag_commit_verified_on"], str) \
                and entry["tag_commit_verified_on"].strip() \
                and not _iso_date(entry["tag_commit_verified_on"]):
            out.append(Violation(file, loc, "P13", "tag_commit_verified_on ISO date",
                                 entry["tag_commit_verified_on"]))
        kind = entry.get("kind", "node")
        if kind not in ("node", "composite"):
            out.append(Violation(file, loc, "P13", "kind node or composite", kind))
        if kind == "composite" and entry.get("transitive_uses_pinned") is not True:
            out.append(Violation(file, loc, "P13", "composite with SHA-pinned transitive uses",
                                 "transitive_uses_pinned not true"))
    return out


def check_publish_registry(policy: Policy, workflows_dir: Path) -> List[Violation]:
    """P12: ``publish_jobs.json`` and ``publish_destinations.json`` agree and are well-formed."""
    out: List[Violation] = []
    jobs_file = policy.paths.get("publish_jobs", "publish_jobs.json")
    dest_file = policy.paths.get("destinations", "publish_destinations.json")
    all_class_perms = {p for perms in PERMISSION_CLASSES.values() for p in perms}
    parsed: Dict[str, Any] = {}

    def job_exists(file: str, job: str) -> bool:
        if file not in parsed:
            try:
                parsed[file] = load_yaml((workflows_dir / file).read_text(encoding="utf-8"))
            except (OSError, UnicodeDecodeError, YamlSubsetError):
                parsed[file] = None
        root = parsed[file]
        jobs = root.get("jobs") if isinstance(root, dict) else None
        return isinstance(jobs, dict) and job in jobs

    for kind, entries in (("jobs", policy.publish_jobs), ("callers", policy.callers)):
        for index, entry in enumerate(entries):
            loc = f"{kind}/{entry.get('job', index)}"
            tier = entry.get("tier")
            if tier not in TIERS:
                out.append(Violation(jobs_file, loc, "P12", "tier release or staging", tier))
            perms = {str(p) for p in _items(entry.get("permissions"))}
            if kind == "jobs":
                if entry.get("file") != "stage-publish.yml":
                    out.append(Violation(jobs_file, loc, "P12", "file stage-publish.yml",
                                         entry.get("file")))
                cls = entry.get("permission_class")
                if cls not in PERMISSION_CLASSES:
                    out.append(Violation(jobs_file, loc, "P12", "known permission class", cls))
                elif perms - set(PERMISSION_CLASSES[cls]):
                    out.append(Violation(jobs_file, loc, "P12", f"permissions of class {cls}",
                                         ", ".join(sorted(perms - set(PERMISSION_CLASSES[cls])))))
            elif perms - all_class_perms:
                out.append(Violation(jobs_file, loc, "P12", "permissions of a known class",
                                     ", ".join(sorted(perms - all_class_perms))))
            if not job_exists(str(entry.get("file")), str(entry.get("job"))):
                out.append(Violation(jobs_file, loc, "P12", "listed job exists",
                                     f"{entry.get('file')}:{entry.get('job')} missing"))

    for index, entry in enumerate(policy.destinations):
        if not isinstance(entry, dict):
            out.append(Violation(dest_file, f"destinations[{index}]", "P12", "an object",
                                 type(entry).__name__))
            continue
        if entry.get("enabled") is not True:
            continue
        name = str(entry.get("name", ""))
        loc = name or f"destinations[{index}]"
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", name):
            out.append(Violation(dest_file, loc, "P12", "name [a-z0-9-]", name or "missing"))
            continue
        module = policy.destinations_dir / (name.replace("-", "_") + ".py")
        if not module.is_file():
            out.append(Violation(dest_file, loc, "P12", "existing module",
                                 f"destinations/{module.name} missing"))
        tier = entry.get("tier")
        cls = entry.get("permission_class")
        if tier not in TIERS:
            out.append(Violation(dest_file, loc, "P12", "tier release or staging", tier))
        if cls not in PERMISSION_CLASSES:
            out.append(Violation(dest_file, loc, "P12", "known permission class", cls))
        if entry.get("environment") != tier:
            out.append(Violation(dest_file, loc, "P12", f"environment {tier}",
                                 entry.get("environment")))
        bad = [c for c in _items(entry.get("credentials"))
               if not re.fullmatch(r"CI_PUBLISH_[A-Z0-9_]+", str(c))]
        if bad:
            out.append(Violation(dest_file, loc, "P12", "credentials named CI_PUBLISH_*",
                                 ", ".join(map(str, bad))))
        if tier in TIERS and cls in PERMISSION_CLASSES and not any(
                j.get("tier") == tier and j.get("permission_class") == cls
                for j in policy.publish_jobs):
            out.append(Violation(dest_file, loc, "P12",
                                 f"a {tier}-tier {cls} job in publish_jobs.json", "none"))
    return out


def check_budgets(policy: Policy) -> List[Violation]:
    """P4: every job budget is 2x its baseline median (worst leg), rounded up to minutes."""
    out: List[Violation] = []
    file = policy.paths.get("budgets", "step_budgets.json")
    if not isinstance(policy.budgets.get("default_job_minutes"), int):
        out.append(Violation(file, "default_job_minutes", "P4", "an integer", "missing"))
    jobs = policy.budgets.get("jobs", {})
    for job, entry in (jobs.items() if isinstance(jobs, dict) else []):
        medians = entry.get("median_s") if isinstance(entry, dict) else None
        if not isinstance(medians, dict) or not medians or \
                not all(isinstance(m, (int, float)) for m in medians.values()):
            out.append(Violation(file, f"jobs/{job}", "P4", "median_s per leg", "missing"))
            continue
        want = math.ceil(2 * max(medians.values()) / 60)
        if entry.get("budget_minutes") != want:
            out.append(Violation(file, f"jobs/{job}", "P4", f"budget_minutes {want}",
                                 entry.get("budget_minutes")))
    return out


def check_pins(policy: Policy) -> List[Violation]:
    """P3: no runner label in the pins file floats (CR-6)."""
    out: List[Violation] = []
    runners = policy.pins.get("runners", {})
    for key, label in (runners.items() if isinstance(runners, dict) else []):
        if str(label).strip().endswith("-latest"):
            out.append(Violation(policy.paths.get("pins", "pins.json"), f"runners/{key}", "P3",
                                 "pinned runner label", label))
    return out


#: Signature of the ``gh api`` runner used by :func:`verify_tags` (swappable in tests).
GhApi = Callable[[str], Optional[dict]]


def _gh_api(endpoint: str) -> Optional[dict]:
    try:
        proc = subprocess.run(["gh", "api", endpoint], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    try:
        data = json.loads(proc.stdout.decode("utf-8"))
    except ValueError:
        return None
    return data if isinstance(data, dict) else None


def verify_tags(policy: Policy, gh_api: GhApi = _gh_api) -> List[Violation]:
    """P13 ``--verify-tags``: each pinned SHA is the upstream tag's commit (CR-7; network).

    Annotated tags are dereferenced. Not part of CI; the reviewer of an
    allowlist change runs it and records ``tag_commit_verified_on``.
    """
    out: List[Violation] = []
    file = policy.paths.get("allowlist", "actions_allowlist.json")
    for action, entry in policy.allowlist.items():
        repo = "/".join(action.split("/")[:2])
        ref = gh_api(f"repos/{repo}/git/ref/tags/{entry.get('version')}")
        obj = ref.get("object") if isinstance(ref, dict) else None
        sha = obj.get("sha") if isinstance(obj, dict) else None
        if isinstance(obj, dict) and obj.get("type") == "tag":
            tag = gh_api(f"repos/{repo}/git/tags/{sha}")
            inner = tag.get("object") if isinstance(tag, dict) else None
            sha = inner.get("sha") if isinstance(inner, dict) else None
        if sha != entry.get("sha"):
            out.append(Violation(file, action, "P13",
                                 f"tag {entry.get('version')} commit {str(entry.get('sha'))[:12]}",
                                 str(sha)[:12] if sha else "lookup failed"))
    return out


def run_gate(workflows: Sequence[Path], *, policy: Policy, workflows_dir: Path,
             scripts_dir: Optional[Path], tags: bool = False,
             gh_api: GhApi = _gh_api) -> List[Violation]:
    """Run every rule; return the violations in a stable order."""
    out: List[Violation] = list(policy.errors)
    for path in workflows:
        out += check_workflow(path, ALLOWED_WORKFLOW_FILES, policy)
    out += check_workflows_dir(workflows_dir, workflows)
    out += check_pins(policy)
    out += check_budgets(policy)
    out += check_allowlist(policy)
    out += check_publish_registry(policy, workflows_dir)
    if scripts_dir is not None:
        out += check_scripts(scripts_dir)
    if tags:
        out += verify_tags(policy, gh_api)
    return out


# --------------------------------------------------------------------------
# Command line
# --------------------------------------------------------------------------

def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="check_workflow_policy.py",
                                description="Workflow policy gate P1-P15.")
    p.add_argument("workflows", nargs="*", type=Path, help="workflow files to check")
    p.add_argument("--workflows-dir", type=Path, default=REPO_ROOT / ".github" / "workflows")
    p.add_argument("--scripts-dir", type=Path, default=_HERE)
    p.add_argument("--no-scripts", action="store_true", help="skip the P11 script scan")
    p.add_argument("--actions-allowlist", type=Path, default=PolicyFiles.actions_allowlist)
    p.add_argument("--publish-jobs", type=Path, default=PolicyFiles.publish_jobs)
    p.add_argument("--publish-destinations", type=Path,
                   default=PolicyFiles.publish_destinations)
    p.add_argument("--destinations-dir", type=Path, default=PolicyFiles.destinations_dir)
    p.add_argument("--budgets", type=Path, default=PolicyFiles.step_budgets)
    p.add_argument("--pins", type=Path, default=PolicyFiles.pins)
    p.add_argument("--verify-tags", action="store_true",
                   help="also check each pinned SHA against its upstream tag (network, gh)")
    p.add_argument("--reporter", default=None)
    return p


def main(argv: Sequence[str]) -> int:
    """Run the gate; print one ``ok:`` line or bounded discrepancy lines; return the exit code."""
    args = _parser().parse_args(list(argv))  # argparse exits 2 on a usage error
    try:
        reporter = get_reporter(args.reporter)
    except ValueError as exc:
        ci_step._emit([f"error: check_workflow_policy.py: {_short(exc)}"])
        return EXIT_USAGE
    start = time.monotonic()
    policy = load_policy(PolicyFiles(args.actions_allowlist, args.publish_jobs,
                                     args.publish_destinations, args.budgets, args.pins,
                                     args.destinations_dir))
    scripts_dir = None if args.no_scripts else args.scripts_dir
    violations = run_gate(args.workflows, policy=policy, workflows_dir=args.workflows_dir,
                          scripts_dir=scripts_dir, tags=args.verify_tags)
    elapsed = round(time.monotonic() - start, 1)
    if not violations:
        summary = f"{len(args.workflows)} workflows, P1-P15"
        if scripts_dir is not None:
            summary += ", scripts/ci scanned"
        ci_step._emit(reporter.step_ok(LABEL, summary, elapsed))
        return EXIT_OK
    lines: List[str] = []
    for line in render_lines(violations):
        lines += reporter.info(line)
    ci_step._emit(lines)
    return EXIT_VIOLATION


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step._emit([f"error: check_workflow_policy.py: internal error ({type(exc).__name__})"])
        sys.exit(EXIT_INTERNAL)
