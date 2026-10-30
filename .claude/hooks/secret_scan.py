#!/usr/bin/env python3
"""Secret scanner - dual mode.

1. As a PreToolUse(Write|Edit|MultiEdit) hook: scans the *content about to be
   written*. If a likely secret is detected it exits 2 to block the write.
2. As a CLI (``python secret_scan.py <file> [<file> ...]``): scans existing
   files on disk; used by the /secret-scan skill and the dep-audit flow.

Detection is pattern based and deliberately conservative-but-loud: it favours
catching real credentials over silence. Findings are logged to
.claude/logs/secret-scan.log. Some codebases already redact secrets at runtime
(e.g. a logging filter) - this hook complements that by stopping them from
entering the repo at all.
"""

from __future__ import annotations

import re
import sys
from collections.abc import Iterable
from pathlib import Path

from _common import REPO_ROOT, allow, append_log, block, edited_path, read_event, tool_input

# name -> compiled pattern. Patterns target high-signal credential shapes.
PATTERNS: dict[str, re.Pattern[str]] = {
    "AWS access key id": re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
    "AWS secret access key": re.compile(r"(?i)aws_secret_access_key\s*[=:]\s*['\"]?[A-Za-z0-9/+=]{40}"),
    "Anthropic API key": re.compile(r"\bsk-ant-[A-Za-z0-9_\-]{20,}"),
    "OpenAI API key": re.compile(r"\bsk-(?:proj-)?[A-Za-z0-9]{20,}"),
    "Google API key": re.compile(r"\bAIza[0-9A-Za-z_\-]{35}\b"),
    "GitHub token": re.compile(r"\bgh[pousr]_[A-Za-z0-9]{36,}\b"),
    "Slack token": re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{10,}"),
    "Private key block": re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |PGP )?PRIVATE KEY-----"),
    "Generic assigned secret": re.compile(
        r"(?i)(password|passwd|secret|token|api[_-]?key)\s*[=:]\s*['\"](?P<value>[^'\"\s]{8,})['\"]"
    ),
    "Postgres URL with password": re.compile(r"postg(?:res|resql)://[^:\s]+:[^@\s]+@"),
}

# Findings whose captured `value` is checked against the memory-address exemption
# below. Only the keyword-driven pattern needs it: every other pattern is anchored
# to a vendor prefix (AKIA, sk-ant-, ghp_, ...) that an address can never produce.
ADDRESS_EXEMPT_TYPES = frozenset({"Generic assigned secret"})

# A 32-/64-bit hex memory address, e.g. 0xDEADBEEF or 0x00007fff5fbff8c0, with
# optional C integer suffix (0xFFFFF80000000000ULL). Underscore digit separators
# are stripped before matching (0x0000_7fff_5fbf_f8c0).
#
# The 16-digit ceiling is the safety property, not a style choice: a 64-bit address
# is at most 16 hex digits, while every hex-encoded credential worth catching is
# longer - AES-128 is 32 digits, AES-256 and Ethereum private keys are 64. So this
# exempts real addresses without opening a hole for `token = "0x<64 hex digits>"`.
_MEM_ADDRESS_RE = re.compile(r"0[xX][0-9a-fA-F]{1,16}[uUlL]{0,3}")

# Substrings that mark an obvious placeholder, so we do not cry wolf.
ALLOWLIST = (
    "example",
    "placeholder",
    "your-",
    "your_",
    "changeme",
    "dummy",
    "xxxx",
    "${",
    "<your",
    "redacted",
    "fake",
    "test",
)

SKIP_SUFFIXES = {".lock", ".png", ".jpg", ".jpeg", ".gif", ".pdf", ".dmp", ".db"}


def _is_placeholder(line: str) -> bool:
    low = line.lower()
    return any(token in low for token in ALLOWLIST)


def _is_memory_address(value: str) -> bool:
    """True when `value` is nothing but a 32-/64-bit hex memory address.

    Checked against the *captured value*, never the whole line, so a line that
    happens to mention an address alongside a real credential is still flagged.
    """
    return bool(_MEM_ADDRESS_RE.fullmatch(value.replace("_", "")))


def scan_text(text: str) -> list[tuple[str, int, str]]:
    """Return (finding_name, line_number, line_excerpt) tuples."""
    hits: list[tuple[str, int, str]] = []
    for lineno, line in enumerate(text.splitlines(), start=1):
        if _is_placeholder(line):
            continue
        for name, pattern in PATTERNS.items():
            match = pattern.search(line)
            if match is None:
                continue
            if name in ADDRESS_EXEMPT_TYPES:
                value = match.groupdict().get("value")
                if value and _is_memory_address(value):
                    continue  # a pointer/offset named `token`, not a credential
            hits.append((name, lineno, line.strip()[:120]))
    return hits


def _content_from_event(event: dict[str, object]) -> str:
    fields = tool_input(event)
    parts: list[str] = []
    for key in ("content", "new_string", "new_str"):
        val = fields.get(key)
        if isinstance(val, str):
            parts.append(val)
    edits = fields.get("edits")
    if isinstance(edits, list):
        for edit in edits:
            if isinstance(edit, dict) and isinstance(edit.get("new_string"), str):
                parts.append(edit["new_string"])
    return "\n".join(parts)


def _hook_mode() -> None:
    event = read_event()
    target = edited_path(event)
    if target and target.suffix.lower() in SKIP_SUFFIXES:
        allow()
    text = _content_from_event(event)
    if not text:
        allow()
    hits = scan_text(text)
    if hits:
        where = target.name if target else "<pending write>"
        for name, lineno, excerpt in hits:
            append_log("secret-scan.log", f"BLOCKED {where}:{lineno} [{name}] {excerpt}")
        report = "\n".join(f"  - line {ln}: {name}" for name, ln, _ in hits)
        block(
            f"Blocked by libbios secret-scan: possible secret in {where}:\n{report}\n"
            "Never commit credentials. Use environment variables / ${VAR} references "
            "(see .mcp.json) or a secrets manager instead."
        )
    allow()


def _cli_mode(paths: Iterable[str]) -> None:
    total = 0
    for raw in paths:
        p = Path(raw)
        if not p.is_absolute():
            p = REPO_ROOT / p
        if not p.is_file() or p.suffix.lower() in SKIP_SUFFIXES:
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for name, lineno, excerpt in scan_text(text):
            total += 1
            rel = p.relative_to(REPO_ROOT) if str(p).startswith(str(REPO_ROOT)) else p
            print(f"{rel}:{lineno}: {name}: {excerpt}")
    if total:
        print(f"\nsecret-scan: {total} potential secret(s) found.")
        sys.exit(1)
    print("secret-scan: clean.")
    sys.exit(0)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        _cli_mode(sys.argv[1:])
    else:
        _hook_mode()
