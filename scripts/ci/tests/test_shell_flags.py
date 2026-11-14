"""Shell under the runner's real flags (milestone 0014, Story 01.0 Task 03; plan E5).

GitHub starts ``shell: bash`` as ``bash --noprofile --norc -e -o pipefail``.
Milestone 0011's ``run_quiet`` was only ever tested without ``-e``, so a bare
``"$@" > "$log"`` killed the step before any diagnostic was printed. These
tests run shell under exactly those flags, against tiny Python stubs for
``cmake``/``ctest``/``apt-get``/``getent`` on a temporary ``PATH``:

- ``ShellFlagsHarness``: the 0011 bug form fails *silently* (proves the harness
  sees the bug); the corrected form (one ``ci_step.py`` call) exits with the
  documented code and prints the RC1 lines before exiting.
- Every ``*.sh`` under ``scripts/ci/`` (none today) must declare
  ``# ci-expected-exit: <n>`` and is run the same way.
- Workflow ``run:`` blocks: every body of the compliant workflow fixtures
  (extracted by ``check_workflow_policy.run_blocks``, Story 02.0 Task 03)
  runs against a stub ``python3``.

Skips with one line when no ``bash`` is found (so it still runs on Windows Git
Bash and macOS when bash is present).
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Dict, Tuple

from ci_test_support import CI_DIR, CI_STEP, FIXTURES, cat_stub_body, neutral_env, write_stub

BASH = shutil.which("bash")
BASH_FLAGS = ("--noprofile", "--norc", "-e", "-o", "pipefail")
_SKIP_NO_BASH = "no bash on PATH; shell-flag tests need bash"

#: Milestone 0011's run_quiet, reduced to its bug: under -e the bare redirect
#: line ends the script when the command fails, so nothing below it runs.
BUG_FORM_0011 = r"""
run_quiet() {
  label=$1; shift
  log="$CI_LOG_DIR/$label.log"
  "$@" > "$log" 2>&1
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "::error title=$label::$label failed (exit $rc)"
    while IFS= read -r line; do
      case "$line" in *error:*) printf 'L: %s\n' "$line" ;; esac
    done < "$log"
    exit "$rc"
  fi
  echo "ok: $label"
}
mkdir -p "$CI_LOG_DIR"
run_quiet build cmake --build build
"""

#: The corrected form: no shell logic, one runner call (plan DD5, RC2).
CORRECTED_FORM = r"""
"$CI_PYTHON" "$CI_STEP_PY" run --label build --patterns cxx -- cmake --build build
"""

#: Documented exit code of the corrected form when the build stub exits 2.
CORRECTED_EXPECTED_EXIT = 2

_EXPECT = re.compile(r"^#\s*ci-expected-exit:\s*(\d+)\s*$", re.MULTILINE)


class _ShellCase(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="ci-shell-test-")
        self.tmp = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)
        self.stubs = self.tmp / "bin"

    def install_stubs(self, build_rc: int) -> None:
        """Stub cmake (prints the cxx_error fixture, exits ``build_rc``) and the other tools."""
        write_stub(self.stubs, "cmake", cat_stub_body(FIXTURES / "cxx_error.log", build_rc))
        write_stub(self.stubs, "ctest", cat_stub_body(FIXTURES / "ctest_44_pass.log", 0))
        write_stub(self.stubs, "apt-get", "import sys\nsys.exit(0)\n")
        write_stub(self.stubs, "getent", "import sys\nprint('127.0.0.1 localhost')\nsys.exit(0)\n")

    def run_bash(self, script: str) -> Tuple[int, str]:
        """Run ``script`` under the runner's flags with the stubs first on PATH."""
        path = self.tmp / "step.sh"
        path.write_text(script, encoding="utf-8", newline="\n")
        env: Dict[str, str] = neutral_env(self.tmp, CI_REPORTER="github")
        env["PATH"] = str(self.stubs) + os.pathsep + env.get("PATH", "")  # bash converts on Windows
        env["CI_PYTHON"] = Path(sys.executable).as_posix()
        env["CI_STEP_PY"] = CI_STEP.as_posix()
        proc = subprocess.run([BASH, *BASH_FLAGS, str(path)], cwd=str(self.tmp), env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              stdin=subprocess.DEVNULL, timeout=60)
        return proc.returncode, proc.stdout.decode("utf-8", errors="replace")

    def assert_rc1_failure(self, rc: int, out: str, expected_exit: int) -> None:
        """The documented exit code, and the RC1 failure lines printed in full before exit."""
        lines = out.splitlines()
        self.assertEqual(rc, expected_exit, out)
        self.assertTrue(lines and lines[0].startswith("::error title=build::"),
                        f"no RC1 annotation before exit:\n{out}")
        self.assertTrue(any(ln.startswith("--- build: first ") for ln in lines), out)
        self.assertTrue(any("use of undeclared identifier" in ln for ln in lines), out)
        m = re.match(r"^::stop-commands::([0-9a-f]{32})$", lines[1] if len(lines) > 1 else "")
        self.assertIsNotNone(m, out)
        self.assertEqual(lines[-1], f"::{m.group(1)}::", "output cut off before the fence closed")


@unittest.skipIf(BASH is None, _SKIP_NO_BASH)
class ShellFlagsHarness(_ShellCase):
    """E5 reproduced: the 0011 bug form is caught; the corrected form passes."""

    def test_BugFormFailsSilentlyUnderRunnerFlags(self) -> None:
        self.install_stubs(build_rc=2)
        rc, out = self.run_bash(BUG_FORM_0011)
        self.assertNotEqual(rc, 0, "the failing build must fail the step")
        self.assertNotIn("::error", out, "-e must have ended the script before the diagnostics")
        self.assertNotIn("use of undeclared identifier", out)

    def test_CorrectedFormPrintsRc1BeforeExit(self) -> None:
        self.install_stubs(build_rc=2)
        rc, out = self.run_bash(CORRECTED_FORM)
        self.assert_rc1_failure(rc, out, CORRECTED_EXPECTED_EXIT)

    def test_CorrectedFormPassingIsOneOkLine(self) -> None:
        self.install_stubs(build_rc=0)
        rc, out = self.run_bash(CORRECTED_FORM)
        self.assertEqual(rc, 0, out)
        lines = out.splitlines()
        self.assertEqual(len(lines), 1, out)
        self.assertTrue(lines[0].startswith("ok: build ("), out)


@unittest.skipIf(BASH is None, _SKIP_NO_BASH)
class ShellScriptsUnderRunnerFlags(_ShellCase):
    """Every ``*.sh`` under scripts/ci runs under -e -o pipefail with its documented exit."""

    def test_EveryShellScript(self) -> None:
        scripts = sorted(p for p in CI_DIR.rglob("*.sh") if "tests" not in p.relative_to(CI_DIR).parts)
        if not scripts:
            self.skipTest("no *.sh under scripts/ci")
        for script in scripts:
            with self.subTest(script=script.name):
                text = script.read_text(encoding="utf-8")
                m = _EXPECT.search(text)
                self.assertIsNotNone(m, f"{script} lacks '# ci-expected-exit: <n>'")
                self.install_stubs(build_rc=2)
                rc, out = self.run_bash(text)
                self.assertEqual(rc, int(m.group(1)), out)

    def test_WorkflowRunBlocks(self) -> None:
        """Every run: body of the compliant workflow fixtures runs under the runner's flags.

        P7 makes each body one ``python3 scripts/ci/<script> ...`` line; a stub
        ``python3`` records its argv, so the test proves the line parses and
        reaches the interpreter with the script path intact under ``-e -o pipefail``.
        """
        from check_workflow_policy import run_blocks
        bodies = [b for wf in sorted((FIXTURES / "workflows" / "good").glob("*.yml"))
                  for b in run_blocks(wf)]
        self.assertTrue(bodies, "no run: bodies in the workflow fixtures")
        record = self.tmp / "argv.txt"
        for body in bodies:
            with self.subTest(body=body):
                write_stub(self.stubs, "python3",
                           "import sys\n"
                           f"open({str(record)!r}, 'w').write(' '.join(sys.argv[1:]))\n")
                rc, out = self.run_bash(body)
                self.assertEqual(rc, 0, out)
                self.assertEqual(record.read_text().split()[0], body.split()[1])


if __name__ == "__main__":
    unittest.main()
