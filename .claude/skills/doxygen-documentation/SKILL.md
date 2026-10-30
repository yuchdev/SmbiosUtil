---
name: doxygen-documentation
description: >
  User-invoked as /doxygen-documentation [path]. Audits every C++ entity
  declaration under path (default src/ + include/) for Doxygen completeness -
  not just presence - and delegates the fix pass to the cpp-code-documenter
  agent. Holds the project's canonical Doxygen templates and worked
  before/after examples so every agent documents entities the same way. Use
  on a legacy module that predates the documentation standard, or after a
  feature lands with thin comments.
allowed-tools: Read, Grep, Glob, Bash, Agent
invocation: /doxygen-documentation [path]
---

# Doxygen Documentation

Bring every C++ entity declaration under `$ARGUMENTS` (default `include/` and
`src/`) up to this project's Doxygen completeness standard, and hold the
canonical templates every agent (`cpp-expert`, `cpp-code-documenter`) must
follow so documentation style stays consistent across the codebase.

This file has two jobs: it is the **skill a user invokes** to run a
documentation pass, and it is the **reference** `cpp-expert` and
`cpp-code-documenter` read for the exact comment shape before writing one.
Read the templates section even if you only came here to run the audit -
without it "documented" silently degrades back into one-line `@brief`s that
restate the entity's name.

## Why "present" is not "complete"

This repo already has `/** @brief ... */` comments on many entities - the
problem the user is flagging is not *absence*, it is *insufficiency*. A
`@brief` that only restates the signature in English is worse than no
comment, because it looks documented in a grep and still leaves the next
reader to open the `.cpp` file to learn what actually matters. Compare:

```cpp
/**
 * @brief Optional forwarding sink for click events.
 */
class IAnalyticsSink {
public:
    virtual ~IAnalyticsSink() = default;
    virtual bool Emit(const ClickEvent& event, AnalyticsError* error = nullptr) = 0;
    virtual bool Flush(AnalyticsError* error = nullptr) = 0;
};
```

This is what every entity in `include/url_shortener/analytics/` looks like
today: the class has one sentence, and `Emit`/`Flush` - the two methods a
caller actually has to get right - have **nothing**. A caller cannot tell
from this header whether `Emit` blocks, whether it can fail after accepting
the event, what `error` means when it returns `false`, or whether `Flush` is
safe to call concurrently with `Emit`. See [Worked examples](#worked-examples)
for the same class brought to the completeness bar below.

## The completeness contract, by entity kind

Every entity you add or touch needs the pieces below. "Non-obvious" is
judged by a reader who knows modern C++ but not this specific module - not
by the author who just wrote it.

| Entity kind | Required |
|---|---|
| Class / struct / interface | `@brief` stating *what it represents or does*, not a reworded name. If it owns a mutex/thread/atomic or captures references, state the thread-safety and lifetime contract here, not buried in a method. |
| Public method / free function | `@brief`; `@param` for every parameter (meaning, units, valid range - not just repeating the type); `@return` meaning of *every* distinct return value, not just "true on success"; `@throws` if it can throw; `@note` for `noexcept`, blocking, or ownership-transfer behavior that isn't obvious from the signature. |
| `bool`-returning method with an `*error` out-param (this project's dominant error convention - see `CLAUDE.md` "Storage: ports and adapters") | `@param[out] error` must state *when* it is written (only on failure, per this project's convention) and that it is left untouched on success; `@return` must enumerate the failure conditions that map to `false`, not just restate "false on failure". |
| Enum class | `@brief` on the enum; one `///<` comment per enumerator on the lines above it stating what that value *means for the caller*, not a restatement of its name. A bare `none`/`ok`/`ignore` value still needs a line saying what "no error" implies downstream. |
| Struct fields | A field needs a doc comment when its name alone doesn't convey: units (bytes, ms, days), a non-default-obvious default, a constraint/invariant ("must be <= `max_retry_attempts`"), or a relationship to a sibling field. A field like `bool enabled = true;` on a config struct still needs one line saying what toggling it actually disables downstream. |
| Constructor capturing a reference/pointer member (common in this codebase - see `AnalyticsService`, `AnalyticsWorker`, `RetentionCleanupJob`) | State the lifetime requirement explicitly: *"The referenced `config`/`queue`/`metrics` must outlive this object; this type does not take ownership."* This is the single most common unstated footgun in the analytics module today. |
| Template parameter | `@tparam` with the constraint the template actually relies on (not just "the type"). |
| Type alias (`using X = Y;`) | Document only when the alias carries meaning beyond its underlying type (`ClickEventId` is "just" a `std::string`, but callers need to know it is an opaque identifier, not validated here, not necessarily a UUID). Skip aliases that are purely a shorthand with no added meaning. |
| Private/implementation-only method | `@brief` only when it does something non-trivial a reader would otherwise have to trace through the body to understand (a retry loop, a non-obvious invariant). A one-line forwarding helper does not need one. |

### What NOT to do

- Do not restate the signature in prose ("Gets the value and returns it").
- Do not fabricate behavior you can't verify from the header + its `.cpp`
  definition. If you cannot tell what a failure case means, say so in the
  agent's report as a flagged item - do not guess a plausible-sounding reason.
- Do not touch logic, signatures, formatting of code, or existing *correct*
  comments "while you're in there". This is a documentation-only pass.
- Do not downgrade an existing comment that is already complete just to match
  a template verbatim - the contract above is a floor, not a rewrite mandate.

## Member and enumerator comment placement

Document data members, struct fields, and enumerators with `///<` placed on
the **lines above** the member, never on the same line as it (no trailing
`int x;  ///< ...`). Wrap the comment to the 80-column limit. This applies to
private members too: every private data member gets a comment stating its
role, what guards it (mutex/atomic), its units or invariants, and - for
reference members - that it is not owned and must outlive the object.

```cpp
    ///< Guards `queue_`, `enqueued_total_` and `stop_`. `mutable` so that
    ///< const methods such as `GetStats()` can lock it.
    mutable std::mutex mutex_;
```

Note: Doxygen itself treats `///<` as documenting the *preceding* entity;
this project deliberately uses it above the member as a house style.

## Reference files

Real, already-compliant headers live in
[`references/`](references/). Read the one matching the entity kind you are
documenting and copy its shape; they are snapshots of
`include/url_shortener/analytics/` and are not compiled.

| Reference | Demonstrates |
|---|---|
| [`analytics_error.hpp`](references/analytics_error.hpp) | Enum with per-enumerator meaning (including "reserved/unused" codes), error struct fields, documented `operator<<` free functions. |
| [`bounded_click_event_queue.hpp`](references/bounded_click_event_queue.hpp) | Class-level thread-safety contract, `noexcept` / blocking / drop behavior in `@return`, fully documented private members (mutex, condition variable, counters, stop flag). |
| [`analytics_worker.hpp`](references/analytics_worker.hpp) | Lifecycle-state enum, reference-capturing constructor lifetime contract, lifecycle-method call rules, private helper methods and members (atomic, thread). |
| [`i_click_event_repository.hpp`](references/i_click_event_repository.hpp) | Storage-port interface using the `bool` + `[out] error` convention: when `error` is written, enumerated `false` cases, virtual destructor. |
| [`retention_cleanup_job.hpp`](references/retention_cleanup_job.hpp) | Result struct whose fields carry invariants and known gaps, injectable-clock type alias, `noexcept` `Run()` outcome documentation. |

If a reference and a template below disagree, the reference wins.

## Canonical templates

### Class / interface

```cpp
/**
 * @brief <What this type represents or coordinates, in one or two sentences.>
 *
 * <Only if applicable:> Thread-safety: <e.g. "All public methods are safe to
 * call concurrently; internal state is guarded by a mutex."> Lifetime:
 * <e.g. "Does not own `repo`/`metrics`; the caller must keep them alive for
 * the lifetime of this object.">
 */
class Example {
public:
    /**
     * @brief <What constructing does, if not obvious - e.g. starts no
     * background work, just stores references.>
     * @param config Validated configuration; `config.flush_interval` must be
     * > 0. The caller must keep `config` alive for the lifetime of this object.
     */
    explicit Example(const Config& config);
};
```

### Method returning `bool` + `*error` out-param (this project's dominant convention)

```cpp
/**
 * @brief Validates and normalizes `query` in place.
 * @param query The query to validate; on success its optional fields are
 * normalized to the bucket's canonical form. Left partially modified if
 * validation fails partway - callers should discard it on `false`.
 * @param[out] error If non-null and validation fails, receives the failure
 * reason (`AnalyticsErrorCode::validation`); left untouched on success.
 * @return `true` if `query` is valid and was normalized; `false` if `from` is
 * after `to`, or `bucket` is set but unsupported for the query's time range.
 */
static bool Validate(AggregateQuery* query, AnalyticsError* error = nullptr);
```

### Enum class

```cpp
/**
 * @brief Error categories for analytics domain operations.
 */
enum class AnalyticsErrorCode {
    ///< No error; the operation completed as requested.
    none,

    ///< A caller-supplied value failed a precondition (e.g. an
    ///< out-of-range timestamp or bucket); the operation did not run.
    validation,

    ///< The bounded event queue rejected or lost the event (full
    ///< or stopped); the event was dropped, not persisted.
    queue,

    ///< The storage backend rejected or failed the operation;
    ///< no data was written.
    repository,

    ///< The background analytics worker failed to process a
    ///< batch after exhausting its retry budget.
    worker,

    ///< A retention/cleanup deletion pass failed partway; some
    ///< rows older than the cutoff may remain.
    retention
};
```

### Struct with fields that need units/constraints stated

```cpp
/**
 * @brief Retry behavior for analytics persistence.
 */
struct AnalyticsRetryConfig {
    ///< Upper bound on persistence retries per batch before the
    ///< worker counts it as a failure.
    std::size_t max_retry_attempts = 3;

    ///< Fixed delay between retry attempts; not exponential.
    std::chrono::milliseconds retry_backoff{100};
};
```

### Reference-capturing constructor (lifetime contract)

```cpp
/**
 * @brief Coordinates click-event enqueueing for the redirect hot path.
 *
 * Lifetime: does not own `queue` or `metrics`; the caller (composition root)
 * must keep both alive for at least as long as this object exists.
 */
class AnalyticsService {
public:
    /**
     * @param config Analytics subsystem configuration; read on every call,
     * not copied, so later mutation of the referenced object is observed.
     * @param queue Destination for normalized click events; must outlive
     * this object.
     * @param metrics Metrics sink updated on every enqueue attempt; must
     * outlive this object.
     */
    AnalyticsService(const AnalyticsConfig& config, IClickEventQueue& queue,
                      IAnalyticsMetrics& metrics) noexcept;
};
```

## Worked examples

### `IAnalyticsSink` - before and after

Before (current state in `include/url_shortener/analytics/i_analytics_sink.hpp`):

```cpp
/**
 * @brief Optional forwarding sink for click events.
 */
class IAnalyticsSink {
public:
    virtual ~IAnalyticsSink() = default;
    virtual bool Emit(const ClickEvent& event, AnalyticsError* error = nullptr) = 0;
    virtual bool Flush(AnalyticsError* error = nullptr) = 0;
};
```

After:

```cpp
/**
 * @brief Optional forwarding sink for click events, in addition to the
 * primary bounded-queue pipeline (see `IClickEventQueue`).
 *
 * Implementations must be safe to call from whatever thread the redirect
 * hot path or the analytics worker runs on; this interface makes no
 * blocking guarantee either way, so an implementation that blocks must say
 * so in its own class comment.
 */
class IAnalyticsSink {
public:
    /**
     * @brief Virtual destructor; implementations may be destroyed through a
     * base `IAnalyticsSink*`.
     */
    virtual ~IAnalyticsSink() = default;

    /**
     * @brief Forwards one normalized click event to the sink.
     * @param event The event to forward; the sink does not take ownership
     * and must not retain a reference beyond the call.
     * @param[out] error If non-null and the call fails, receives the
     * failure reason; left untouched on success.
     * @return `true` if the event was accepted by the sink; `false` if the
     * sink rejected or failed to forward it. A `false` return never throws
     * and never affects the caller's redirect response.
     */
    virtual bool Emit(const ClickEvent& event, AnalyticsError* error = nullptr) = 0;

    /**
     * @brief Requests the sink flush any buffered events immediately.
     * @param[out] error If non-null and the flush fails, receives the
     * failure reason; left untouched on success.
     * @return `true` if the flush completed (or there was nothing to
     * flush); `false` if the flush failed. Best-effort: a `false` return
     * must never be treated as data loss by the caller without separately
     * checking sink-specific state.
     */
    virtual bool Flush(AnalyticsError* error = nullptr) = 0;
};
```

### `ClickEvent` - struct with an undocumented field block and a bare `bool` method

Before:

```cpp
/**
 * @brief Normalized redirect-attempt analytics event.
 */
struct ClickEvent {
    ClickEventId event_id;
    Timestamp occurred_at{};
    std::string slug;
    std::string link_id;
    std::string domain;
    uint16_t status_code = 0;
    std::string referrer;
    std::string user_agent;
    std::string client_id_hash;

    bool Validate(AnalyticsError* error = nullptr) const;
};
```

After:

```cpp
/**
 * @brief Normalized redirect-attempt analytics event, ready to enqueue or
 * persist. Produced by `ClickEventBuilder::Build`, which applies
 * sanitization and client-id hashing before any `ClickEvent` is constructed.
 */
struct ClickEvent {
    ///< Opaque identifier for this event; caller-assigned, not
    ///< validated here.
    ClickEventId event_id;

    ///< Wall-clock time the redirect attempt completed.
    Timestamp occurred_at{};

    ///< Short-link slug that was resolved (without the `/r/` prefix).
    std::string slug;

    ///< Stable identifier of the resolved link, independent of the slug.
    std::string link_id;

    ///< Host header of the inbound request, already sanitized/truncated.
    std::string domain;

    ///< HTTP status code the redirect responded with.
    uint16_t status_code = 0;

    ///< Sanitized `Referer` header value, truncated to
    ///< `SanitizationLimits::max_referrer_bytes`.
    std::string referrer;

    ///< Sanitized `User-Agent` header value, truncated to
    ///< `SanitizationLimits::max_user_agent_bytes`.
    std::string user_agent;

    ///< HMAC-SHA256 hash of the client identifier; the raw identifier
    ///< is never stored (see `ClientIdHasher`).
    std::string client_id_hash;

    /**
     * @brief Checks that this event's required fields are populated and
     * within the limits `ClickEventBuilder` is expected to have already
     * enforced.
     * @param[out] error If non-null and validation fails, receives the
     * failure reason (`AnalyticsErrorCode::validation`); left untouched on
     * success.
     * @return `true` if every required field is non-empty and within
     * limits; `false` otherwise.
     */
    bool Validate(AnalyticsError* error = nullptr) const;
};
```

## Steps (when invoked as `/doxygen-documentation [path]`)

1. **Inventory**: count entities in scope so the agent (and you) know the
   size of the job:
   ```bash
   grep -rnE "^\s*(class|struct|enum class|enum)\s+\w+" $ARGUMENTS
   ```
   Note which files already carry comments on *every* entity - they still
   need the completeness check (a present `@brief` is not sufficient), but
   are lower-risk to re-touch than files with none at all.
2. **Delegate** to the **`cpp-code-documenter`** agent with the target path.
   It reads this skill's templates above, classifies each entity against
   the completeness contract, and brings every entity up to the bar via
   comment-only edits - flagging anything it cannot verify from the header
   plus its `.cpp` definition rather than guessing.
3. **Verify no regressions**: comments are non-executable, but a malformed
   insertion (unterminated `/**`, a misplaced `///<`) is a compile error.
   Confirm the affected target(s) still build:
   ```bash
   cmake --build cmake-build
   ```
4. Report the agent's summary.

## Output

```
## Doxygen Documentation - <path>
Scanned: N files, M entities (classes/structs C, enums E, methods/functions F)
Documented: X new/upgraded
Skipped (already complete, left as-is): <list, or none>
Flagged (behavior unclear from header + .cpp - needs SME input): <entity -> what's unclear>
Verification: cmake --build cmake-build -> <pass/fail>
```

If the build fails after insertion, stop and hand the failure to `cpp-expert`
before re-running this skill.

## Completion checklist

- [ ] Inventory run before any edit
- [ ] Every touched entity meets the completeness contract table above, not
  just "has a `@brief`"
- [ ] Every `bool` + `*error` out-param method states when `error` is written
  and enumerates its `false` conditions
- [ ] Every reference/pointer-capturing constructor states the lifetime
  contract
- [ ] No logic, signatures, or existing complete comments changed - diff is
  comments only
- [ ] Affected target(s) still build after insertion
- [ ] Flagged/ambiguous items reported, not silently guessed
