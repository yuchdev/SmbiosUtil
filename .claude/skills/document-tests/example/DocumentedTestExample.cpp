/**
 * Reference specimen for /document-tests - the Doxygen-comment format, one
 * per classification.
 *
 * This is a *teaching example*, not a collected test (it lives under
 * `.claude/`, outside any real `tests/unit`, `tests/integration`, or
 * `tests/e2e` directory, and its filename doesn't match any pattern
 * `scripts/document_tests.py` discovers, so no build tool and no codemod run
 * ever touches it). Each test case below shows two things at once:
 *
 *   1. the structural signal `scripts/document_tests.py` keys on to classify
 *      it, and
 *   2. the exact Doxygen comment the codemod emits, as described in
 *      `../references/classification-guide.md`.
 *
 * Doxygen-comment contract (enforced by the codemod's title regex
 * `^\[(Unit|Mock|Integration|E2E)\] .+: verifies .+\.$`):
 *
 *   [<Class>] <context>: verifies <one sentence ending in a period>.
 *
 *   Scenario: ...
 *   Boundaries: ...
 *   On failure, first check: ...
 *
 * The namespace below, `example::widgetkit`, is a deliberately fictional
 * library so nothing here looks like it should resolve against your own
 * code. Read the *shape* - a real test including the header under test and
 * asserting on real behavior - and copy the Doxygen comments, not the names.
 *
 * Note on classification: the codemod decides by *directory/filename first,
 * then body markers*. This file sits in an unrecognised directory with a
 * non-matching filename, so a real run of the codemod would never even see
 * it; the `[Integration]` tag on the third test below is illustrative of
 * where that test would live in a real project (`tests/integration/`), not a
 * live classification. In a real project each of these four test cases would
 * instead live in its own conventional directory (`tests/unit/`,
 * `tests/integration/`, or `tests/e2e/`) and the directory signal alone would
 * settle it.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "widgetkit/proof_repository.h"
#include "widgetkit/proof_reviewer.h"
#include "widgetkit/streak_calculator.h"

namespace example::widgetkit {
namespace {

using ::testing::Return;

// ----------------------------------------------------------------------- //
// [Unit] - pure logic, no mocks, no I/O. Signal: no mock/subprocess markers.
// ----------------------------------------------------------------------- //
/**
 * [Unit] streak calculator: verifies a missed day resets the streak to zero.
 *
 * Scenario:
 *   - Given a StreakCalculator with a current streak of 5 and a 2-day gap
 *   - When StreakCalculator::nextStreak(5, /*gapDays=*/2) executes
 *   - Then the result is confirmed to be 0
 *
 * Boundaries:
 *   - Focus: the gap-resets-streak branch of nextStreak
 *   - Fixtures/params: none
 *   - Scope: pure function, no I/O
 *
 * On failure, first check:
 *   - The gap-day threshold compared against inside StreakCalculator::nextStreak
 */
TEST(StreakCalculatorTest, MissedDayResetsStreakToZero) {
    EXPECT_EQ(StreakCalculator::nextStreak(5, 2), 0);
}

// ----------------------------------------------------------------------- //
// [Mock] - unit-scoped, but a collaborator is mocked. Signal: GoogleMock markers.
// ----------------------------------------------------------------------- //
class MockProofRepository : public ProofRepository {
public:
    MOCK_METHOD(std::optional<Proof>, findById, (const std::string& id), (override));
    MOCK_METHOD(void, markRejected, (const std::string& id), (override));
};

/**
 * [Mock] proof review: verifies rejecting a proof calls the repository's reject method.
 *
 * Scenario:
 *   - Given a ProofRepository mocked to return a pending proof for "abc123"
 *   - When ProofReviewer::reject(repository, "abc123") executes
 *   - Then the result is confirmed by verifying repository.markRejected("abc123") was called
 *
 * Boundaries:
 *   - Focus: ProofReviewer::reject's delegation to the repository
 *   - Fixtures/params: repository (mocked)
 *   - Scope: only ProofReviewer runs for real; the repository it calls is a GoogleMock mock
 *
 * On failure, first check:
 *   - Whether ProofReviewer::reject still calls markRejected rather than erase
 */
TEST(ProofReviewerTest, RejectingProofCallsRepositoryMarkRejected) {
    MockProofRepository repository;
    EXPECT_CALL(repository, findById("abc123")).WillOnce(Return(Proof::pending("abc123")));
    EXPECT_CALL(repository, markRejected("abc123"));

    ProofReviewer(repository).reject("abc123");
}

// ----------------------------------------------------------------------- //
// [Integration] - two+ real components wired together, external edge sandboxed.
// Signal: this file would live under tests/integration/ in a real project;
// no mock/subprocess markers.
// ----------------------------------------------------------------------- //
/**
 * [Integration] goal store: verifies a saved goal reloads by goal id.
 *
 * Scenario:
 *   - Given a real InMemoryGoalStore and a real GoalMapper wired together
 *   - When a Goal is saved and then reloaded by id "goal-1"
 *   - Then the reloaded Goal is confirmed to equal the one that was saved
 *
 * Boundaries:
 *   - Focus: GoalStore + GoalMapper round trip
 *   - Fixtures/params: none
 *   - Scope: real store and mapper; no repository mocks, no filesystem I/O
 *
 * On failure, first check:
 *   - Whether GoalMapper::toRecord / fromRecord round-trip lost a field
 */
TEST(GoalStoreTest, RoundTripsByGoalId) {
    InMemoryGoalStore store{GoalMapper{}};
    const Goal goal{"goal-1", "Run 5k"};

    store.save(goal);
    const Goal reloaded = store.findById("goal-1");

    EXPECT_EQ(goal, reloaded);
}

// ----------------------------------------------------------------------- //
// [E2E] - a full user-facing journey driven end to end. Signal: a spawned
// subprocess invoking the real built binary.
// ----------------------------------------------------------------------- //
/**
 * [E2E] CLI: verifies the --version flag prints the build version.
 *
 * Scenario:
 *   - Given the built `widgetkit-cli` binary on PATH
 *   - When `widgetkit-cli --version` is invoked via boost::process
 *   - Then stdout is confirmed to contain the release version string
 *
 * Boundaries:
 *   - Focus: the CLI entry point's --version handling
 *   - Fixtures/params: none
 *   - Scope: the whole process is spawned and run for real; nothing is mocked
 *
 * On failure, first check:
 *   - Whether the CLI's argument parser still recognizes --version before subcommand dispatch
 */
TEST(CliTest, VersionFlagPrintsBuildVersion) {
    boost::process::ipstream out;
    boost::process::child proc("widgetkit-cli --version", boost::process::std_out > out);

    std::string line;
    std::getline(out, line);
    proc.wait();

    EXPECT_THAT(line, ::testing::HasSubstr(kReleaseVersion));
}

}  // namespace
}  // namespace example::widgetkit
