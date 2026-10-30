# Test classification & Doxygen-comment guide

Backs `/document-tests`. The `scripts/document_tests.py` codemod classifies
**mechanically** from structural signals; this guide gives the **semantic**
definitions you use to hand-resolve the `Ambiguous classification` cases it
flags, plus the Doxygen-comment format the script emits.

## How the script classifies (match its logic when overriding)

Directory and filename first, then body markers:

1. Path contains an `e2e` directory, or the filename ends `_e2e_test.cpp`/
   `_e2e_test.cc` → **E2E**, regardless of body content.
2. Path contains an `integration` directory, or the filename ends
   `_integration_test.cpp`/`_integration_test.cc`/`IT.cpp`/`IT.cc` → **E2E**
   if the body uses an end-to-end marker, else **Integration**.
3. Path contains a `unit`/`test`/`tests` directory (or an unrecognised
   layout) → **E2E** if the body uses an end-to-end marker; else **Mock** if
   the body uses a GoogleMock marker; else **Unit**.
4. Unrecognised directory → same body-based guess as step 3, **flagged `ambiguous`**.

Body markers the script looks for (case-insensitive substring match):

- **End-to-end:** `std::system(`, ` system(`, `popen(`, `fork(`, `execve(`,
  `boost::process`, `subprocess::`, `curl_easy_`, `httplib::client`,
  `grpc::CreateChannel`, `::socket(`.
- **GoogleMock:** `MOCK_METHOD`, `EXPECT_CALL(`, `NiceMock<`, `StrictMock<`,
  `ON_CALL(`, `::testing::Mock`, `ReturnRef(`, `WillOnce(`, `WillRepeatedly(`.

`ambiguous` means "no directory convention matched" — the script guessed from
the body only. Those are the cases you must read and confirm.

## Semantic definitions (the ground truth for resolving ambiguity)

| Class           | Real meaning                                                                                                                  | libbios examples                                                              |
|-----------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| **Unit**        | Pure logic, no mocking, no filesystem/network/device I/O. Isolated by construction, not by mocking.                          | A parsing routine, a mapper, a pure math/domain function.                               |
| **Mock**        | Unit-scoped but a collaborator is replaced with a GoogleMock fake. Tests behaviour *around* an external boundary without hitting it. | A repository test with the storage backend mocked; a service test with the clock/network client mocked. |
| **Integration** | Two or more real components wired together, externals mocked or sandboxed at the process edge. Not a pure unit; not a full user flow. | A service + an in-memory store; a real parser over a fixture file.                       |
| **E2E**         | A full user-facing journey driven end to end — a spawned subprocess, a real network/RPC call, or an end-to-end CLI/service action — even if some internals are mocked. | A CLI invocation via `boost::process`; a service test that opens a real socket. |

Resolution rule of thumb when the script flags ambiguous:

- Mocks present but only one unit under test → **Mock**, not Integration.
- Real collaborators wired together, external edge mocked/sandboxed → **Integration**.
- A subprocess/socket/RPC marker anywhere in the body → **E2E**, regardless of mocks.
- No mocks, no subprocess/socket markers, pure inputs/outputs → **Unit**.

The precedence when signals conflict is **E2E > Integration > Mock > Unit** — the
outer boundary being exercised is what a future reader cares about most. This
matches the classification model already published in `test-documenter`
(`.claude/agents/test-documenter.md`); keep the two in sync if either changes.

## Doxygen-comment format the script emits

Title line must match:
`^\[(Unit|Mock|Integration|E2E)\] <context>: verifies <one sentence>.$`
followed by **Scenario**, **Boundaries**, and **On failure, first check** stanzas.

A full worked specimen — one correctly-documented test per classification,
each also exhibiting the body signal the codemod keys on — is in
[../example/DocumentedTestExample.cpp](../example/DocumentedTestExample.cpp).

### Good

```cpp
/**
 * [Unit] discount tier: verifies subtotal below threshold maps to NONE.
 *
 * Scenario:
 *   - Given a subtotal of 49.99 with no mocks or I/O
 *   - When DiscountTier::fromSubtotal(49.99) executes
 *   - Then the result is confirmed to be DiscountTier::kNone
 *
 * Boundaries:
 *   - Focus: the <50.00 branch of the discount ladder
 *   - Fixtures/params: none
 *   - Scope: pure function, no I/O
 *
 * On failure, first check:
 *   - The 50.00 / 100.00 cutoffs in DiscountTier::fromSubtotal
 */
TEST(DiscountTierTest, BelowThresholdIsNone) { ... }
```

### Bad (why)

```cpp
// no [Class] tag, no 'verifies', no stanzas
/** Tests the discount thing. */
TEST(DiscountTierTest, Stuff) { ... }
```

The bad one fails the title regex, carries no classification, and gives an
on-call reader nothing to act on when it breaks.

## Boundaries of this skill

Doxygen comments only. Never change test logic, fixtures, assertions, macro
arguments, or includes. Leave any test that already has a custom
(non-generated) doc comment untouched unless the user explicitly asked to
`--force` that specific file.
