#!/usr/bin/env python3
"""Smoke-check that a command prints one well-formed structured document.

Usage:
    python3 -I check_structured_output.py --format json|xml|csv|yaml|text -- <command> [args...]

Runs <command>, requires exit status 0, and checks its stdout:

* json: must be a list of objects, each with a string "type" and an object
  "fields";
* xml: must parse to exactly one root element, and must contain no
  ``<!DOCTYPE`` or ``<!ENTITY`` declaration (rejected before parsing, so no
  entity is ever expanded - defence in depth against CWE-776; the formatter
  never emits either);
* csv: must parse (``csv`` module, RFC 4180) to the header row
  ``type,field_name,field_value`` followed by rows of exactly three columns;
* yaml: the standard library has no YAML parser, so this is a *shape* check
  only, not a parse: the document starts with ``---``, every structure begins
  with a ``- type: "..."`` line, and every other non-blank line is indented
  under it. Use PyYAML (``yaml.safe_load``) for a full parse where available;
* text: no structure to parse (and ``text`` is not sanitised, so it is not
  required to be UTF-8); stdout must merely be non-empty.

Every format except text must also be valid UTF-8.

Privacy: the command reads the live machine's SMBIOS table, so its stdout
can hold serial numbers and UUIDs. This script prints only
``ok: <format>, <N> structures`` (``<N> rows`` for csv) on success, and
on failure only a reason and (for parse errors) the error position - never a parsed value and never
the raw stdout.

Standard library only. Exit status: 0 on success, 1 on any check failure,
2 on a usage error.
"""

import argparse
import csv
import io
import json
import subprocess
import sys
import xml.etree.ElementTree as ElementTree


def fail(reason: str) -> int:
    """Print a value-free failure reason to stderr and return exit status 1."""
    print(f"FAIL: {reason}", file=sys.stderr)
    return 1


def check_json(text: str) -> int:
    """Validate the JSON document shape; return its structure count or raise ValueError."""
    try:
        document = json.loads(text)
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid JSON at line {error.lineno}, column {error.colno}") from None
    if not isinstance(document, list):
        raise ValueError("JSON root is not an array")
    for index, entry in enumerate(document):
        if not isinstance(entry, dict):
            raise ValueError(f"JSON element {index} is not an object")
        if not isinstance(entry.get("type"), str):
            raise ValueError(f"JSON element {index} has no string \"type\"")
        if not isinstance(entry.get("fields"), dict):
            raise ValueError(f"JSON element {index} has no object \"fields\"")
    return len(document)


def check_xml(text: str) -> int:
    """Validate the XML document (one root element); return its child count or raise ValueError.

    A DOCTYPE or ENTITY declaration is rejected before parsing, so ElementTree
    never expands an internal entity (billion laughs, CWE-776, threat model
    SR-9). The xml formatter never emits either, so this rejects nothing valid.
    """
    for marker in ("<!DOCTYPE", "<!ENTITY"):
        if marker in text:
            raise ValueError(f"XML contains a {marker} declaration, which is not allowed")
    try:
        root = ElementTree.fromstring(text)
    except ElementTree.ParseError as error:
        line, column = error.position
        raise ValueError(f"invalid XML at line {line}, column {column}") from None
    return len(root)


def check_csv(text: str) -> int:
    """Validate the CSV header and row widths; return the number of data rows or raise ValueError.

    CSV has one row per field and no structure delimiter (two consecutive
    structures of the same type are indistinguishable), so rows are counted.
    """
    reader = csv.reader(io.StringIO(text, newline=""), strict=True)
    try:
        rows = list(reader)
    except csv.Error:
        raise ValueError(f"invalid CSV at line {reader.line_num}") from None
    if not rows or rows[0] != ["type", "field_name", "field_value"]:
        raise ValueError("CSV header row is not type,field_name,field_value")
    for index, row in enumerate(rows[1:], start=2):
        if len(row) != 3:
            raise ValueError(f"CSV row {index} has {len(row)} columns, expected 3")
    return len(rows) - 1


def check_yaml_shape(text: str) -> int:
    """Shape-check (not parse) the YAML document; return the structure count or raise ValueError."""
    lines = text.splitlines()
    if not lines or lines[0] != "---":
        raise ValueError("YAML document does not start with ---")
    structures = 0
    for number, line in enumerate(lines[1:], start=2):
        if not line.strip():
            continue
        if line.startswith("- type: \"") and line.endswith("\""):
            structures += 1
        elif not line.startswith("  ") or structures == 0:
            raise ValueError(f"YAML line {number} is neither a '- type:' item nor indented under one")
    return structures


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--format", required=True, choices=("json", "xml", "csv", "yaml", "text"))
    parser.add_argument("command", nargs=argparse.REMAINDER,
                        help="the command to run, after a literal --")
    args = parser.parse_args(argv)

    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("no command given after --")

    try:
        completed = subprocess.run(command, stdout=subprocess.PIPE, check=False)
    except OSError as error:
        return fail(f"cannot run command ({error.strerror})")
    if completed.returncode != 0:
        return fail(f"command exited with status {completed.returncode}")

    if args.format == "text":
        if not completed.stdout.strip():
            return fail("stdout is empty")
        print("ok: text, non-empty")
        return 0

    try:
        text = completed.stdout.decode("utf-8")
    except UnicodeDecodeError as error:
        return fail(f"stdout is not valid UTF-8 at byte offset {error.start}")

    checkers = {"json": check_json, "xml": check_xml, "csv": check_csv, "yaml": check_yaml_shape}
    try:
        count = checkers[args.format](text)
    except ValueError as error:
        return fail(str(error))

    unit = "rows" if args.format == "csv" else "structures"
    print(f"ok: {args.format}, {count} {unit}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
