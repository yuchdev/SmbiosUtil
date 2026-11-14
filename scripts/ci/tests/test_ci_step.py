"""Tests for the CI step runner and its report contract (milestone 0014, Story 01.0 Task 03).

Covers ``scripts/ci/ci_step.py`` (RC1 output grammar, RC2 interface, RC3
record, RC5 best-effort), ``scripts/ci/reporters/`` (ADR 0005 neutral events,
every backend) and ``scripts/ci/env_names.py`` (neutral names, exit 2), plus
the P11 "no platform syntax in scripts" scan.

Assertions pin the RC1 *shape* and *bounds* (fixed prefixes ``ok: ``,
``::error title=``, ``--- ``, ``::warning title=best-effort: ``, line and byte
ceilings), not exact wording, so a wording polish does not break them.
Stdlib only; no network; no host cmake/ctest; synthetic logs only.

Run: ``python3 scripts/ci/run_tests.py`` (all of ``scripts/ci/tests``).
"""

from __future__ import annotations

import contextlib
import io
import os
import re
import subprocess
import sys
import tempfile
import time
import tracemalloc
import unittest
from pathlib import Path
from typing import List, Optional, Tuple
from unittest import mock

from ci_test_support import (CI_DIR, FIXTURES, RC1_ANNOTATION_MAX, RC1_LINE_WIDTH,
                             RC1_MAX_BYTES, RC1_MAX_DIAG, RC1_MAX_LINES, RC1_MAX_TAIL,
                             RC3_KEYS, cat_argv, cat_stub_body, neutral_env, path_with,
                             rc1_bytes, run_cli, stub_argv, write_stub)

import ci_step  # noqa: E402  (ci_test_support put scripts/ci on sys.path)
import env_names  # noqa: E402
import reporters  # noqa: E402
from reporters.github import GithubReporter, escape_annotation  # noqa: E402
from reporters.plain import PlainReporter  # noqa: E402
from reporters.teamcity import TeamcityReporter, escape_value  # noqa: E402

BACKENDS = ("github", "teamcity", "plain")
_OK_LINE = re.compile(r"^ok: (?P<label>[^:]+?)(?:: (?P<summary>.*))? \((?P<elapsed>\d+\.\d)s\)$")
_TOKEN = re.compile(r"^::stop-commands::(?P<token>[0-9a-f]{32})$")


def _diag_header(label: str) -> "re.Pattern[str]":
    return re.compile(rf"^--- {re.escape(label)}: first (\d+) of (\d+) diagnostic lines ---$")


def _tail_header(label: str) -> "re.Pattern[str]":
    return re.compile(rf"^--- {re.escape(label)}: last (\d+) lines ---$")


def split_fenced(test: unittest.TestCase, backend: str,
                 lines: List[str]) -> Tuple[str, List[str], Optional[str]]:
    """Split one rendered failing step into (annotation, excerpt, github token).

    Asserts the backend's fence structure: the annotation is the only line
    outside the fence, and every excerpt line is inside it.
    """
    test.assertGreaterEqual(len(lines), 1)
    head, rest = lines[0], lines[1:]
    if not rest:
        return head, [], None
    if backend == "github":
        m = _TOKEN.match(rest[0])
        test.assertIsNotNone(m, f"excerpt does not open with stop-commands: {rest[0]!r}")
        token = m.group("token")
        test.assertEqual(rest[-1], f"::{token}::", "excerpt does not close with the same token")
        body = rest[1:-1]
        test.assertFalse(any(f"::{token}::" in ln for ln in body), "token resumes inside the fence")
        return head, body, token
    if backend == "teamcity":
        test.assertEqual(rest[0], "##teamcity[disableServiceMessages]")
        test.assertEqual(rest[-1], "##teamcity[enableServiceMessages]")
        body = rest[1:-1]
        test.assertFalse(any("##teamcity[" in ln for ln in body),
                         "a service message survived inside the fence")
        return head, body, None
    return head, rest, None  # plain: no fence (ADR 0005 table: "none needed")


def parse_excerpt(test: unittest.TestCase, label: str,
                  body: List[str]) -> Tuple[List[str], List[str], int]:
    """Return (diagnostic lines, tail lines, diagnostic total) from an RC1 excerpt."""
    diag: List[str] = []
    tail: List[str] = []
    total = 0
    target: Optional[List[str]] = None
    dh, th = _diag_header(label), _tail_header(label)
    for line in body:
        m = dh.match(line)
        if m:
            test.assertIsNone(target, "diagnostic block must come first")
            target, total = diag, int(m.group(2))
            test.assertLessEqual(int(m.group(1)), total)
            continue
        m = th.match(line)
        if m:
            test.assertIsNot(target, tail, "tail header printed twice")
            target = tail
            continue
        test.assertIsNotNone(target, f"excerpt line before any block header: {line!r}")
        target.append(line)
    for line in diag:
        test.assertRegex(line, r"^L\d+: ")
    return diag, tail, total


def assert_bounded(test: unittest.TestCase, lines: List[str]) -> None:
    """RC1 hard ceiling: 64 lines, 16 KiB, and no control characters on any line."""
    test.assertLessEqual(len(lines), RC1_MAX_LINES)
    test.assertLessEqual(rc1_bytes(lines), RC1_MAX_BYTES)
    for line in lines:
        test.assertNotRegex(line, r"[\x00-\x08\x0a-\x1f\x7f]", f"control char in {line!r}")


class _TmpCase(unittest.TestCase):
    """Gives every test its own temporary directory, removed afterwards."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="ci-step-test-")
        self.tmp = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)


class ReportContractTests(_TmpCase):
    """RC1/RC3/RC5 behaviour of ``ci_step.py run`` from the command line."""

    def test_SuccessIsOneLine(self) -> None:
        child = self.tmp / "noisy.log"
        child.write_text("".join(f"passing tool output line {i}\n" for i in range(500)))
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                env = neutral_env(self.tmp, CI_RESULTS_FILE=str(self.tmp / f"{backend}.jsonl"))
                r = run_cli(self.tmp, ["run", "--reporter", backend, "--label", "build",
                                       "--summary", "3 targets", "--", *cat_argv(child, 0)],
                            env=env)
                self.assertEqual(r.rc, 0)
                self.assertEqual(len(r.lines), 1, r.lines)
                m = _OK_LINE.match(r.lines[0])
                self.assertIsNotNone(m, r.lines[0])
                self.assertEqual((m.group("label"), m.group("summary")), ("build", "3 targets"))
                self.assertEqual(r.stderr, "")
                self.assertEqual(len(r.records), 1)
                rec = r.records[0]
                self.assertEqual(set(rec), RC3_KEYS)
                self.assertEqual((rec["outcome"], rec["mode"], rec["exit"], rec["label"]),
                                 ("ok", "hard", 0, "build"))
                self.assertIsNone(rec["first_diagnostic"])
                # The full log stays on disk (RC2), byte-identical.
                self.assertEqual((self.tmp / "logs" / "build.log").read_bytes(), child.read_bytes())

    def test_FailureIsBounded(self) -> None:
        r = run_cli(self.tmp, ["run", "--reporter", "github", "--label", "build",
                               "--patterns", "cxx", "--", *cat_argv(FIXTURES / "cxx_error.log", 2)])
        self.assertEqual(r.rc, 2, "hard mode must return the child's exit code unchanged")
        assert_bounded(self, r.lines)
        head, body, _ = split_fenced(self, "github", r.lines)
        self.assertTrue(head.startswith("::error title=build::build failed (exit 2, "), head)
        self.assertIn("use of undeclared identifier", head, "annotation carries the first diagnostic")
        diag, tail, total = parse_excerpt(self, "build", body)
        self.assertLessEqual(len(diag), RC1_MAX_DIAG)
        self.assertLessEqual(len(tail), RC1_MAX_TAIL)
        self.assertEqual(total, 3)
        self.assertEqual(len(diag), 3)
        self.assertFalse(any("memory_error_information" in d for d in diag),
                         "the cxx set must match diagnostic shapes, not the bare word 'error'")
        rec = r.records[0]
        self.assertEqual((rec["outcome"], rec["exit"]), ("fail", 2))
        self.assertIn("use of undeclared identifier", rec["first_diagnostic"])

    def test_FailureIsBoundedAtTheCeiling(self) -> None:
        """Worst case: 300 long diagnostics and long tail lines hit every RC1 limit."""
        log = self.tmp / "flood.log"
        with open(log, "w", encoding="utf-8") as fh:
            for i in range(300):
                fh.write(f"src/f{i}.cpp:{i}:1: error: " + "x" * 900 + "\n")
                fh.write("plain context line " + "y" * 900 + "\n")
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                env = neutral_env(self.tmp, CI_RESULTS_FILE=str(self.tmp / f"{backend}.jsonl"))
                r = run_cli(self.tmp, ["run", "--reporter", backend, "--label", "flood",
                                       "--patterns", "cxx", "--", *cat_argv(log, 1)], env=env)
                self.assertEqual(r.rc, 1)
                assert_bounded(self, r.lines)
                _, body, _ = split_fenced(self, backend, r.lines)
                diag, tail, total = parse_excerpt(self, "flood", body)
                self.assertEqual(total, 300, "the header reports the true diagnostic count")
                self.assertGreater(len(diag), 0)
                self.assertLessEqual(len(diag), RC1_MAX_DIAG)
                self.assertLessEqual(len(tail), RC1_MAX_TAIL)
                for line in body:
                    self.assertLessEqual(len(line), RC1_LINE_WIDTH)

    def test_TailDoesNotRepeat(self) -> None:
        fixture = FIXTURES / "cxx_error.log"
        r = run_cli(self.tmp, ["run", "--reporter", "github", "--label", "build",
                               "--patterns", "cxx", "--", *cat_argv(fixture, 1)])
        _, body, _ = split_fenced(self, "github", r.lines)
        diag, tail, _ = parse_excerpt(self, "build", body)
        diag_numbers = {int(d[1:d.index(":")]) for d in diag}
        diag_texts = {d.split(": ", 1)[1] for d in diag}
        self.assertFalse(diag_texts & set(tail), "a diagnostic line is repeated in the tail")
        # Precisely: the tail is the last lines of the log minus those already shown.
        log_lines = fixture.read_text(encoding="utf-8").splitlines()
        expected = [t for n, t in enumerate(log_lines, 1) if n not in diag_numbers][-len(tail):]
        self.assertEqual(tail, expected)
        self.assertEqual(len(tail), RC1_MAX_TAIL)

    def test_HugeLogBounded(self) -> None:
        """50 MB of child output (incl. one 4 MB line): bounded output, peak < 64 MiB."""
        huge = self.tmp / "huge.log"  # generated here, never committed
        block = "".join(f"[{i}/9999] Building CXX object some/long/path/unit_{i:05d}.cpp.o "
                        f"with flags -O2 -Wall -Wextra\n" for i in range(10000)).encode()
        target = 50 * 1024 * 1024
        with open(huge, "wb") as fh:
            written = 0
            while written < target // 2:
                fh.write(block)
                written += len(block)
            fh.write(b"z" * (4 * 1024 * 1024) + b"\n")  # one overlong line
            fh.write(b"src/late.cpp:9:9: error: synthetic late diagnostic\n")
            written += 4 * 1024 * 1024
            while written < target:
                fh.write(block)
                written += len(block)
        self.assertGreaterEqual(huge.stat().st_size, target)

        out = io.StringIO()
        start = time.monotonic()
        cpu_start = time.process_time()
        tracemalloc.start()
        try:
            with contextlib.redirect_stdout(out):
                result = ci_step.run_step("huge", cat_argv(huge, 1), patterns=["cxx"],
                                          best_effort_reason=None, summary=None,
                                          log_dir=self.tmp / "logs", reporter=GithubReporter(),
                                          results_file=self.tmp / "results.jsonl")
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        elapsed = time.monotonic() - start
        cpu = time.process_time() - cpu_start
        lines = out.getvalue().splitlines()
        self.assertEqual((result.exit, result.outcome), (1, "fail"))
        self.assertLess(peak, 64 * 1024 * 1024, f"peak traced memory {peak} bytes")
        # The runner's own CPU time is the bound (the scan is streaming, not
        # quadratic). Wall-clock is only a hang guard: it is load-dependent and
        # failed at machine load > 50 although the test passes alone in ~20 s.
        self.assertLess(cpu, 30.0)
        self.assertLess(elapsed, 300.0)
        assert_bounded(self, lines)
        _, body, _ = split_fenced(self, "github", lines)
        diag, tail, total = parse_excerpt(self, "huge", body)
        self.assertEqual(total, 1)
        self.assertIn("synthetic late diagnostic", diag[0])
        self.assertEqual(len(tail), RC1_MAX_TAIL)

    def test_HostileOutputCannotInject(self) -> None:
        fixture = FIXTURES / "hostile.log"
        r = run_cli(self.tmp, ["run", "--reporter", "github", "--label", "hostile", "--",
                               *cat_argv(fixture, 1)])
        self.assertEqual(r.rc, 1)
        assert_bounded(self, r.lines)
        head, body, token = split_fenced(self, "github", r.lines)
        self.assertIsNotNone(token)
        hostile_bytes = fixture.read_bytes()
        self.assertNotIn(token.encode(), hostile_bytes)
        self.assertNotIn(token.encode(), (self.tmp / "logs" / "hostile.log").read_bytes())
        # Every hostile command-looking line is inside the fence, never outside it.
        joined = "\n".join(body)
        for needle in ("::error::", "::add-mask::", "::stop-commands::x", "::warning::",
                       "teamcity[", "::x::"):
            self.assertIn(needle, joined, f"{needle!r} missing from the excerpt")
        # Outside the fence there is exactly the annotation (split_fenced checked
        # that the fence spans every other line).
        self.assertEqual(len(r.lines), len(body) + 3)
        # Annotation: escaped (% -> %25), single line, cut.
        self.assertTrue(head.startswith("::error title=hostile::"))
        message = head.split("::", 2)[2]
        self.assertIn("100%25", message)
        self.assertIn("%250A", message, "a literal %0A in tool output must not decode to LF")
        self.assertNotIn("\r", head)
        # ESC sequences and CR are stripped, invalid UTF-8 is replaced, not crashed on.
        self.assertNotIn("\x1b", r.raw.decode("utf-8"))
        self.assertNotIn(b"\r", r.raw)
        self.assertIn("error: coloured diagnostic", joined)
        self.assertNotIn("[31m", joined)
        self.assertIn("�", joined)
        # A fresh token per call: never reused, never derived from the output.
        r2 = run_cli(self.tmp, ["run", "--reporter", "github", "--label", "hostile", "--",
                                *cat_argv(fixture, 1)])
        _, _, token2 = split_fenced(self, "github", r2.lines)
        self.assertNotEqual(token, token2)

    def test_BestEffortNeverFails(self) -> None:
        r = run_cli(self.tmp, ["run", "--reporter", "github", "--label", "docs",
                               "--best-effort", "network may be down", "--",
                               *cat_argv(FIXTURES / "cxx_error.log", 2)])
        self.assertEqual(r.rc, 0)
        assert_bounded(self, r.lines)
        head, body, _ = split_fenced(self, "github", r.lines)
        self.assertTrue(head.startswith("::warning title=best-effort: docs::"), head)
        self.assertIn("network may be down", head)
        self.assertIn("(exit 2)", head)
        self.assertFalse(any(ln.startswith("::error") for ln in r.lines))
        self.assertGreater(len(body), 0, "RC5: the RC1 excerpt is printed with the warning")
        rec = r.records[0]
        self.assertEqual((rec["outcome"], rec["mode"], rec["exit"]), ("warn", "best-effort", 2))

    def test_BestEffortPassingIsOk(self) -> None:
        r = run_cli(self.tmp, ["run", "--label", "docs", "--best-effort", "why", "--",
                               sys.executable, "-c", "pass"])
        self.assertEqual(r.rc, 0)
        self.assertEqual(len(r.lines), 1)
        self.assertRegex(r.lines[0], _OK_LINE)
        self.assertEqual((r.records[0]["outcome"], r.records[0]["mode"]), ("ok", "best-effort"))

    def test_MissingExecutable(self) -> None:
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                env = neutral_env(self.tmp, CI_RESULTS_FILE=str(self.tmp / f"{backend}.jsonl"))
                r = run_cli(self.tmp, ["run", "--reporter", backend, "--label", "tool", "--",
                                       "ci-step-test-no-such-command-xyz"], env=env)
                self.assertEqual(r.rc, 127)
                self.assertEqual(len(r.lines), 1, r.lines)
                self.assertIn("cannot start ci-step-test-no-such-command-xyz", r.lines[0])
                self.assertIn("exit 127", r.lines[0])
                self.assertNotIn("Traceback", r.stderr + "\n".join(r.lines))
                self.assertEqual((r.records[0]["outcome"], r.records[0]["exit"]), ("fail", 127))

    @unittest.skipIf(os.name == "nt", "POSIX signals only")
    def test_SignalExit(self) -> None:
        r = run_cli(self.tmp, ["run", "--label", "killed", "--", sys.executable, "-c",
                               "import os, signal; print('about to die'); "
                               "os.kill(os.getpid(), signal.SIGTERM)"])
        self.assertEqual(r.rc, 143)
        self.assertEqual(r.records[0]["exit"], 143)
        self.assertEqual(r.records[0]["outcome"], "fail")

    def test_CtestBothWordings(self) -> None:
        stubs = self.tmp / "bin"
        for fixture in ("ctest_31_pass.log", "ctest_44_pass.log"):
            with self.subTest(fixture=fixture):
                write_stub(stubs, "ctest", cat_stub_body(FIXTURES / fixture, 0))
                env = neutral_env(self.tmp, PATH=path_with(stubs),
                                  CI_RESULTS_FILE=str(self.tmp / f"{fixture}.jsonl"))
                r = run_cli(self.tmp, ["run", "--reporter", "github", "--label", "ctest",
                                       "--patterns", "ctest", "--",
                                       *stub_argv(stubs, "ctest"), "-L", "gui-ci"], env=env)
                self.assertEqual(r.rc, 0, r.lines)
                self.assertEqual(len(r.lines), 1)
                self.assertRegex(r.lines[0], _OK_LINE)
                self.assertEqual(r.records[0]["outcome"], "ok")

    def test_PatternSetsMatchFixtures(self) -> None:
        cases = (("boost_fail.log", "boost", 2, 'error: in "SyntheticSuite/SizeDecodesTestCase"'),
                 ("asan_report.log", "sanitizer", 2, "ERROR: AddressSanitizer: heap-buffer-overflow"),
                 ("cxx_error.log", "cxx", 3, "use of undeclared identifier"))
        for fixture, sets, count, first in cases:
            with self.subTest(fixture=fixture):
                env = neutral_env(self.tmp, CI_RESULTS_FILE=str(self.tmp / f"{fixture}.jsonl"))
                r = run_cli(self.tmp, ["run", "--reporter", "github", "--label", "t",
                                       "--patterns", sets, "--", *cat_argv(FIXTURES / fixture, 1)],
                            env=env)
                _, body, _ = split_fenced(self, "github", r.lines)
                diag, _, total = parse_excerpt(self, "t", body)
                self.assertEqual((len(diag), total), (count, count))
                self.assertIn(first, diag[0])
                self.assertIn(first, r.records[0]["first_diagnostic"])

    def test_UsageErrorsExit2(self) -> None:
        cases = (["run", "--label", "x"],                                  # no command
                 ["run", "--label", "bad;label", "--", sys.executable, "-c", "pass"],
                 ["run", "--label", "x", "--max-diag", "21", "--", sys.executable, "-c", "pass"],
                 ["run", "--label", "x", "--tail", "41", "--", sys.executable, "-c", "pass"],
                 ["run", "--label", "x", "--patterns", "nope", "--", sys.executable, "-c", "pass"],
                 ["run", "--reporter", "bogus", "--label", "x", "--", sys.executable, "-c", "pass"])
        for args in cases:
            with self.subTest(args=args):
                r = run_cli(self.tmp, args)
                self.assertEqual(r.rc, 2)
                self.assertEqual(r.records, [], "a usage error must not write a record")
                self.assertNotIn("Traceback", r.stderr)
        # An unknown backend named through the environment is also a usage error.
        r = run_cli(self.tmp, ["run", "--label", "x", "--", sys.executable, "-c", "pass"],
                    env=neutral_env(self.tmp, CI_REPORTER="bogus"))
        self.assertEqual((r.rc, r.records), (2, []))

    #: Every CR-5 denied name, plus one example per denied prefix (Story 02.0
    #: Task 04). Built by concatenation where a name is a P11 needle.
    DENIED_EXAMPLES = (
        "ACTIONS_STEP_DEBUG", "ACTIONS_RUNNER_DEBUG", "RUNNER" + "_DEBUG",
        "GITHUB" + "_ENV", "GITHUB" + "_OUTPUT", "GITHUB" + "_PATH", "GITHUB" + "_STATE",
        "GITHUB" + "_STEP_SUMMARY", "CI_OUTPUT_FILE",
        "ACTIONS_ID_TOKEN_REQUEST_TOKEN", "ACTIONS_ID_TOKEN_REQUEST_URL",
        "ACTIONS_RUNTIME_TOKEN", "ACTIONS_RUNTIME_URL", "ACTIONS_CACHE_URL",
        "ACTIONS_RESULTS_URL",
        "CI_PUBLISH_TOKEN", "CI_PUBLISH_S3_ROLE", "AWS_SECRET_ACCESS_KEY",
        "AWS_SESSION_TOKEN", "AWS_WEB_IDENTITY_TOKEN_FILE",
        "TEAMCITY" + "_BUILD_PROPERTIES_FILE", "TEAMCITY" + "_SECURE_PUBLISH",
        "BUILD_SECURE_TOKEN",
    )
    _ENV_PROBE = ("import json, os, sys; names = json.loads(sys.argv[1]); "
                  "print('inherited=' + json.dumps(sorted(n for n in names if n in os.environ)))"
                  "; sys.exit(1)")

    def test_ChildDoesNotInheritDebugSwitches(self) -> None:
        """CR-5: no denied name or prefix reaches a child (extended by Story 02.0 Task 04)."""
        import json
        names = list(self.DENIED_EXAMPLES) + ["CI_KEEP_ME"]
        env = neutral_env(self.tmp, **{n: "secret-value" for n in names})
        r = run_cli(self.tmp, ["run", "--label", "envcheck", "--", sys.executable, "-c",
                               self._ENV_PROBE, json.dumps(names)], env=env)
        self.assertEqual(r.rc, 1)
        self.assertIn('inherited=["CI_KEEP_ME"]', "\n".join(r.lines))
        # Library-level check, case-insensitive like a Windows environment.
        scrubbed = reporters.child_environment({**{n: "v" for n in names},
                                                "aws_region": "v", "Github" + "_Env": "v"})
        self.assertEqual(sorted(scrubbed), ["CI_KEEP_ME"])

    def test_PassEnvKeepsOnlyTheNamedDeniedName(self) -> None:
        """CR-5 positive case: ``pass_env`` keeps one denied name by exact name."""
        import json
        names = ["AWS_WEB_IDENTITY_TOKEN_FILE", "AWS_SECRET_ACCESS_KEY",
                 "ACTIONS_ID_TOKEN_REQUEST_TOKEN"]
        env = neutral_env(self.tmp, **{n: "x" for n in names})
        out = io.StringIO()
        with mock.patch.dict(os.environ, env, clear=True), contextlib.redirect_stdout(out):
            result = ci_step.run_step(
                "passenv", [sys.executable, "-c", self._ENV_PROBE, json.dumps(names)],
                patterns=["python"], best_effort_reason=None, summary=None,
                log_dir=self.tmp / "logs", reporter=PlainReporter(),
                results_file=self.tmp / "r.jsonl", pass_env=("AWS_WEB_IDENTITY_TOKEN_FILE",))
        self.assertEqual(result.exit, 1)
        self.assertIn('inherited=["AWS_WEB_IDENTITY_TOKEN_FILE"]', out.getvalue())
        self.assertEqual(reporters.child_environment({"A": "1"}, ("A",)), {"A": "1"})

    def test_SelfTestIsOneOkLine(self) -> None:
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                r = run_cli(self.tmp, ["self-test", "--reporter", backend])
                self.assertEqual(r.rc, 0, r.lines)
                self.assertEqual(len(r.lines), 1)
                self.assertTrue(r.lines[0].startswith("ok: ci_step self-test ("), r.lines[0])

    def test_HelpWorksIsolatedFromAnyDirectory(self) -> None:
        r = run_cli(self.tmp, ["--help"], cwd=self.tmp)
        self.assertEqual(r.rc, 0)
        self.assertIn("self-test", "\n".join(r.lines))

    def test_InternalErrorIsOneLineExit70(self) -> None:
        out = io.StringIO()
        env = neutral_env(self.tmp)
        with mock.patch.dict(os.environ, env, clear=True), \
                mock.patch.object(ci_step, "run_step", side_effect=RuntimeError("boom")), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()) as err:
            rc = ci_step.main(["run", "--reporter", "github", "--label", "x", "--",
                               sys.executable, "-c", "pass"])
        self.assertEqual(rc, 70)
        self.assertEqual(out.getvalue().splitlines(),
                         ["::error title=ci_step::internal error (RuntimeError)"])
        self.assertNotIn("Traceback", err.getvalue())


class ReporterBackendTests(_TmpCase):
    """ADR 0005: every backend renders every neutral event, and fences hostile output."""

    def _hostile_run(self, backend: str, line: str) -> Tuple[List[str], List[str]]:
        log = self.tmp / f"{backend}-input.log"
        log.write_text(f"src/a.cpp:1:1: error: real diagnostic\n{line}\ntrailing context\n",
                       encoding="utf-8")
        env = neutral_env(self.tmp, CI_RESULTS_FILE=str(self.tmp / f"{backend}.jsonl"))
        r = run_cli(self.tmp, ["run", "--reporter", backend, "--label", "step", "--patterns",
                               "cxx", "--", *cat_argv(log, 1)], env=env)
        self.assertEqual(r.rc, 1)
        assert_bounded(self, r.lines)
        head, body, _ = split_fenced(self, backend, r.lines)
        return [head], body

    def _assert_hostile_contained(self, line: str) -> None:
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                head, body = self._hostile_run(backend, line)
                expected_head = {"github": "::error title=step::",
                                 "teamcity": "##teamcity[buildProblem description='",
                                 "plain": "error: "}[backend]
                self.assertTrue(head[0].startswith(expected_head), head[0])
                self.assertIn("real diagnostic", head[0])
                # Diagnostic-block lines carry an "L<n>: " prefix; tail lines do not.
                shown = [re.sub(r"^L\d+: ", "", b) for b in body]
                if backend == "teamcity":
                    # Neutralised as well as fenced: it cannot close the fence early.
                    self.assertIn(line.replace("##teamcity[", "## teamcity["), shown)
                    self.assertFalse(any("##teamcity[" in b for b in body))
                else:
                    self.assertIn(line, shown, "the hostile line is shown as excerpt data")

    def test_HostileErrorLineInEveryBackend(self) -> None:
        self._assert_hostile_contained("::error::injected annotation")

    def test_HostileTeamcityLineInEveryBackend(self) -> None:
        self._assert_hostile_contained("##teamcity[buildProblem description='injected']")
        self._assert_hostile_contained("##teamcity[enableServiceMessages]")

    def test_EveryBackendRendersEveryNeutralEvent(self) -> None:
        excerpt = ["--- s: last 1 lines ---", "context"]
        summary_file = self.tmp / "summary.md"
        artifacts = self.tmp / "artifacts"
        env = neutral_env(self.tmp, CI_SUMMARY_FILE=str(summary_file),
                          CI_ARTIFACT_DIR=str(artifacts))
        expected = {
            "github": ("::error title=s::s failed (exit 3, 1.5s): diag",
                       "::warning title=best-effort: s::why (exit 2)",
                       "::error title=ci_step::internal error (KeyError)"),
            "teamcity": ("##teamcity[buildProblem description='s failed (exit 3, 1.5s): diag' "
                         "identity='s']",
                         "##teamcity[message text='best-effort: s: why (exit 2)' status='WARNING']",
                         "##teamcity[buildProblem description='internal error (KeyError)' "
                         "identity='ci_step']"),
            "plain": ("error: s failed (exit 3, 1.5s): diag",
                      "warning: best-effort: s: why (exit 2)",
                      "error: ci_step: internal error (KeyError)"),
        }
        for backend in BACKENDS:
            with self.subTest(backend=backend), mock.patch.dict(os.environ, env, clear=True):
                rep = reporters.get_reporter(backend)
                self.assertEqual(rep.name, backend)
                fail_head, warn_head, internal = expected[backend]
                self.assertEqual(rep.step_ok("s", "2 files", 1.5), ["ok: s: 2 files (1.5s)"])
                self.assertEqual(rep.step_ok("s", "", 1.5), ["ok: s (1.5s)"])
                self.assertEqual(rep.step_skip("s", "no display"), ["skip: s: no display"])
                fail = rep.step_fail("s", 3, 1.5, "diag", excerpt)
                warn = rep.step_warn("s", "why", 2, excerpt)
                self.assertEqual(fail[0], fail_head)
                self.assertEqual(warn[0], warn_head)
                self.assertEqual(rep.internal_error("KeyError"), [internal])
                # Excerpt is fenced by exactly begin/end_untrusted_block.
                for rendered in (fail, warn):
                    _, body, _ = split_fenced(self, backend, rendered)
                    self.assertEqual(body, excerpt)
                # No excerpt -> no empty fence.
                self.assertEqual(rep.step_fail("s", 3, 1.5, "diag", []), [fail_head])
                begin, end = rep.begin_untrusted_block(), rep.end_untrusted_block()
                if backend == "github":
                    token = _TOKEN.match(begin[0]).group("token")
                    self.assertEqual(end, [f"::{token}::"])
                    self.assertNotEqual(rep.begin_untrusted_block(), begin, "token reused")
                    rep.end_untrusted_block()
                elif backend == "teamcity":
                    self.assertEqual((begin, end), (["##teamcity[disableServiceMessages]"],
                                                    ["##teamcity[enableServiceMessages]"]))
                else:
                    self.assertEqual((begin, end), ([], []))
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    rep.job_summary("# Summary line\n\nbody")
                if backend == "github":
                    self.assertEqual(out.getvalue(), "")
                    self.assertEqual(summary_file.read_text(encoding="utf-8"),
                                     "# Summary line\n\nbody\n")
                elif backend == "teamcity":
                    written = artifacts / "ci-summary.md"
                    self.assertEqual(written.read_text(encoding="utf-8"), "# Summary line\n\nbody\n")
                    printed = out.getvalue().splitlines()
                    self.assertEqual(len(printed), 2)
                    self.assertTrue(printed[0].startswith("##teamcity[publishArtifacts '"))
                    self.assertEqual(printed[1], "##teamcity[buildStatus text='# Summary line']")
                else:
                    self.assertEqual(out.getvalue(), "# Summary line\n\nbody\n")

    def test_GithubSummaryFallsBackToStdout(self) -> None:
        out = io.StringIO()
        with mock.patch.dict(os.environ, neutral_env(self.tmp), clear=True), \
                contextlib.redirect_stdout(out):
            GithubReporter().job_summary("x")
        self.assertEqual(out.getvalue(), "x\n")

    def test_GithubEscaping(self) -> None:
        self.assertEqual(escape_annotation("50% done\r\nnext"), "50%25 done%0D%0Anext")
        self.assertEqual(len(escape_annotation("a" * 500)), RC1_ANNOTATION_MAX)
        # Cut before escaping, so an escape sequence is never split.
        self.assertTrue(escape_annotation("a" * 159 + "%").endswith("%25"))
        head = GithubReporter().step_fail("a:b,c", 1, 0.0, "d", [])[0]
        self.assertTrue(head.startswith("::error title=a%3Ab%2Cc::"), head)

    def test_TeamcityEscaping(self) -> None:
        self.assertEqual(escape_value("it's [x] | y\nz\r"), "it|'s |[x|] || y|nz|r")
        self.assertEqual(escape_value("\u0085  "), "|x|l|p")
        head = TeamcityReporter().step_fail("s", 1, 0.0, "##teamcity[x]", [])[0]
        self.assertEqual(head.count("##teamcity["), 1, head)

    def test_GetReporterSelection(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertIsInstance(reporters.get_reporter(None), PlainReporter)
            self.assertIsInstance(reporters.get_reporter("github"), GithubReporter)
            with self.assertRaises(ValueError):
                reporters.get_reporter("jenkins")
        with mock.patch.dict(os.environ, {"CI_REPORTER": "teamcity"}, clear=True):
            self.assertIsInstance(reporters.get_reporter(None), TeamcityReporter)
            self.assertIsInstance(reporters.get_reporter("plain"), PlainReporter)

    def test_CiReporterEnvSelectsBackendOnCli(self) -> None:
        env = neutral_env(self.tmp, CI_REPORTER="github")
        r = run_cli(self.tmp, ["run", "--label", "x", "--", sys.executable, "-c",
                               "print('src/a.c:1:1: error: e'); raise SystemExit(4)"], env=env)
        self.assertEqual(r.rc, 4)
        self.assertTrue(r.lines[0].startswith("::error title=x::"))


class EnvNamesTests(unittest.TestCase):
    """ADR 0005 neutral-name accessor."""

    def test_MissingRequiredNameExits2(self) -> None:
        err = io.StringIO()
        with mock.patch.dict(os.environ, {}, clear=True), contextlib.redirect_stderr(err):
            with self.assertRaises(SystemExit) as cm:
                env_names.get("CI_PUBLISH_TOKEN", required=True)
        self.assertEqual(cm.exception.code, 2)
        self.assertEqual(len(err.getvalue().splitlines()), 1)
        self.assertIn("CI_PUBLISH_TOKEN", err.getvalue())

    def test_MissingRequiredNameExits2FromProcess(self) -> None:
        code = ("import sys; sys.path.insert(0, sys.argv[1]); import env_names; "
                "env_names.get('CI_COMMIT', required=True)")
        env = {k: v for k, v in os.environ.items() if not k.startswith("CI_")}
        proc = subprocess.run([sys.executable, "-I", "-c", code, str(CI_DIR)], env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(proc.returncode, 2)
        self.assertEqual(len(proc.stderr.splitlines()), 1)
        self.assertNotIn(b"Traceback", proc.stderr)

    def test_OptionalMissingIsEmptyAndDefaultsApply(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(env_names.get("CI_PUBLISH_TOKEN"), "")
            self.assertEqual(env_names.get("CI_REPORTER"), "plain")
            self.assertEqual(env_names.get("CI_PLATFORM"), "local")
            self.assertTrue(env_names.get("CI_RESULTS_FILE", required=True))
        with mock.patch.dict(os.environ, {"CI_REPORTER": "github"}, clear=True):
            self.assertEqual(env_names.get("CI_REPORTER"), "github")

    def test_UnknownNameIsRejected(self) -> None:
        with self.assertRaises(KeyError):
            env_names.get("GITHUB" + "_SHA")

    def test_TableIsNeutral(self) -> None:
        self.assertTrue(all(n.startswith("CI_") for n in env_names.NAMES))
        for required in ("CI_PLATFORM", "CI_REPORTER", "CI_LOG_DIR", "CI_RESULTS_FILE",
                         "CI_SUMMARY_FILE", "CI_PUBLISH_TOKEN"):
            self.assertIn(required, env_names.NAMES)


# --- P11: no platform syntax in scripts -------------------------------------------

#: P11 needles (02.0 Task 03 / ADR 0005). Built by concatenation so this file
#: does not itself contain them verbatim.
P11_NEEDLES = ("$" + "{{", "%" + "teamcity.", "##" + "teamcity[", "::" + "error",
               "::" + "warning", "::" + "stop-commands")
P11_ENV_READ = re.compile(r"\b(?:GITHUB|RUNNER|TEAMCITY)_[A-Z0-9_]*")
#: P11's own exceptions, plus tests/: the harness must contain hostile input and
#: assert on rendered platform syntax. The policy gate (02.0 Task 03) needs the
#: same tests/ exception or it will flag this harness.
P11_EXEMPT = ("reporters", "env_names.py", "tests")


def p11_violations(root: Path) -> List[str]:
    """Return ``path:line: needle`` for every P11 violation under ``root``."""
    found: List[str] = []
    for path in sorted(root.rglob("*")):
        rel = path.relative_to(root)
        if not path.is_file() or rel.parts[0] in P11_EXEMPT or "__pycache__" in rel.parts:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for number, line in enumerate(text.splitlines(), 1):
            hits = [n for n in P11_NEEDLES if n in line] + P11_ENV_READ.findall(line)
            found.extend(f"{rel.as_posix()}:{number}: {h}" for h in hits)
    return found


class P11PolicyTests(_TmpCase):
    """P11: only reporters/ and env_names.py may know platform syntax or variables."""

    def test_ScriptsCiHasNoPlatformSyntax(self) -> None:
        self.assertEqual(p11_violations(CI_DIR), [])

    def test_ScannerDetectsEveryNeedle(self) -> None:
        """Positive control, so the scan above cannot pass vacuously."""
        (self.tmp / "reporters").mkdir()
        (self.tmp / "reporters" / "ok.py").write_text("x = '" + P11_NEEDLES[3] + "'\n")
        bad = self.tmp / "bad.py"
        bad.write_text("\n".join(f"s = '{n}'" for n in P11_NEEDLES)
                       + "\nos.environ['RUNNER" + "_TEMP']\n", encoding="utf-8")
        hits = p11_violations(self.tmp)
        self.assertEqual(len(hits), len(P11_NEEDLES) + 1, hits)
        self.assertTrue(all(h.startswith("bad.py:") for h in hits))


class RunStepTimeoutTests(_TmpCase):
    """``run_step(timeout_s=...)`` (added by Story 02.0 Task 02 for network steps).

    Classification: unit. On failure first check: ``ci_step._run_with_timeout``'s
    signal path (process group on POSIX) and ``EXIT_TIMEOUT``.
    """

    def test_StallIsKilledAndStillPrintsRc1(self) -> None:
        child = "import sys, time\nprint('stalling now', flush=True)\ntime.sleep(60)\n"
        out = io.StringIO()
        start = time.monotonic()
        with contextlib.redirect_stdout(out):
            result = ci_step.run_step("stall", [sys.executable, "-c", child], patterns=["apt"],
                                      best_effort_reason=None, summary=None,
                                      log_dir=self.tmp / "logs", reporter=PlainReporter(),
                                      results_file=self.tmp / "r.jsonl", timeout_s=1.0)
        self.assertLess(time.monotonic() - start, 20.0)
        self.assertEqual(result.exit, ci_step.EXIT_TIMEOUT)
        self.assertEqual(result.outcome, "fail")
        lines = out.getvalue().splitlines()
        self.assertTrue(lines[0].startswith("error: stall failed (exit 124"), lines)
        self.assertIn("timed out after 1s", lines[0])
        self.assertIn("stalling now", lines)

    def test_NoTimeoutKeepsChildExitCode(self) -> None:
        with contextlib.redirect_stdout(io.StringIO()):
            result = ci_step.run_step("quick", [sys.executable, "-c", "raise SystemExit(3)"],
                                      patterns=["apt"], best_effort_reason=None, summary=None,
                                      log_dir=self.tmp / "logs", reporter=PlainReporter(),
                                      results_file=self.tmp / "r.jsonl", timeout_s=30.0)
        self.assertEqual(result.exit, 3)


if __name__ == "__main__":
    unittest.main()
