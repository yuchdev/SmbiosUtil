---
name: document-tests
description: User-invoked as /document-tests [path]. Documents every GoogleTest `TEST`/`TEST_F`/`TEST_P`/`TYPED_TEST` case under path (default the whole repo) - classifies each as Unit, Mock, Integration, or E2E and inserts a standardized Scenario/Boundaries/On-failure Doxygen comment via the document_tests.py codemod, then delegates flagged/ambiguous cases to the test-documenter agent. Use to bring an undocumented or partially-documented test suite up to a consistent standard.
allowed-tools: Read, Grep, Glob, Bash, Agent
invocation: /document-tests [path]
---

# Document Tests

Document every `TEST`/`TEST_F`/`TEST_P`/`TYPED_TEST`/`TYPED_TEST_P` case under
`$ARGUMENTS` (default the whole repo) with a standardized classification +
Scenario/Boundaries/On-failure Doxygen comment.

Unlike the `python` preset's equivalent, `scripts/document_tests.py` here is
**regex- and brace-counting-based, not AST-based** - the stdlib has no C++
parser. It is a best-effort heuristic that reads structural signals (test
directory, filename suffix, GoogleMock/subprocess markers in the test body),
not test semantics. It will occasionally misparse unusual formatting; that is
what step 2's `Ambiguous` list and the `test-documenter` agent are for.

**When resolving `Ambiguous classification` items** (step 2 below), use
[references/classification-guide.md](references/classification-guide.md) — it
gives the semantic Unit/Mock/Integration/E2E definitions the script cannot infer,
the resolution rules of thumb, and the exact Doxygen-comment format (with a
good/bad example) the codemod emits.

## Steps

1. **Preview**: `python scripts/document_tests.py $ARGUMENTS --check`
   Read the summary: total test cases, classification counts, any `Skipped
   (custom doc comment present)` and `Ambiguous classification` lists.
2. **Decide whether the agent is needed**:
   - If there are zero `Skipped` and zero `Ambiguous` entries, the change is
     purely mechanical - apply directly: `python scripts/document_tests.py $ARGUMENTS`.
   - Otherwise, spawn the **`test-documenter`** agent with the target path and
     the preview output. It applies the script, then hand-resolves every
     `Ambiguous` item by reading the test body, and leaves every `Skipped`
     (custom doc comment) item untouched unless the user asked to standardize
     that specific file.
3. **Verify no regressions**: build the touched test targets with whatever
   this project uses - e.g. `cmake --build build --target <test-target>`, or
   a full `cmake --build build` if targets aren't isolated. A broken
   insertion shows up as a compile error, not a passing/failing test, so this
   check is cheap and catches it immediately.
4. Report the summary the agent (or the direct script run) produced.

## Output

```
## Test Documentation - <path>
Scanned: N files, M test cases
[Unit] a  [Mock] b  [Integration] c  [E2E] d
Documented: X new/updated
Skipped (custom doc comments, left as-is): <list, or none>
Ambiguous - resolved: <file::qualname -> classification>
Verification: <build command> -> <pass/fail>
```

If the build step fails, stop and hand the failure to `cpp-expert` before
re-running this skill - do not re-apply the codemod over a file that's already
broken. If the user wants hand-written doc comments standardized too, re-run
with the `test-documenter` agent and explicit permission to `--force` the
specific files named.

## Completion checklist

- [ ] `--check` preview run before any apply - counts, `Skipped`, and `Ambiguous` lists reviewed
- [ ] Every `Skipped (custom doc comment present)` item left untouched, unless the user explicitly asked to `--force` that specific file
- [ ] Every `Ambiguous classification` item resolved by reading the actual test body - not left at the script's best-effort guess
- [ ] The project's test-target build run after apply and passed (no compile errors)
- [ ] Diff is Doxygen comments only - no test logic, fixtures, assertions, macro arguments, or includes changed
- [ ] Classification counts, skipped list, and ambiguous resolutions all included in the final report
