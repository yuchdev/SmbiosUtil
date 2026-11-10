#!/usr/bin/env python3
"""Smoke-check that a command prints one well-formed JSON or XML document.

Usage:
    python3 -I check_structured_output.py --format json|xml -- <command> [args...]

Runs <command>, requires exit status 0, and parses its stdout:

* json: must be a list of objects, each with a string "type" and an object
  "fields";
* xml: must parse to exactly one root element.

Privacy: the command reads the live machine's SMBIOS table, so its stdout
can hold serial numbers and UUIDs. This script prints only
``ok: <format>, <N> structures`` on success, and on failure only a reason
and (for parse errors) the error position - never a parsed value and never
the raw stdout.

Standard library only. Exit status: 0 on success, 1 on any check failure,
2 on a usage error.
"""

import argparse
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
    """Validate the XML document (one root element); return its child count or raise ValueError."""
    try:
        root = ElementTree.fromstring(text)
    except ElementTree.ParseError as error:
        line, column = error.position
        raise ValueError(f"invalid XML at line {line}, column {column}") from None
    return len(root)


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--format", required=True, choices=("json", "xml"))
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

    try:
        text = completed.stdout.decode("utf-8")
    except UnicodeDecodeError as error:
        return fail(f"stdout is not valid UTF-8 at byte offset {error.start}")

    try:
        count = check_json(text) if args.format == "json" else check_xml(text)
    except ValueError as error:
        return fail(str(error))

    print(f"ok: {args.format}, {count} structures")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
