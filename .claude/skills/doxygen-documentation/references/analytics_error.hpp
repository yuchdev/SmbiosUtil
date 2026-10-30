#pragma once

#include <ostream>
#include <string>

namespace url_shortener::analytics
{

/**
 * @brief Error categories for analytics domain operations.
 */
enum class AnalyticsErrorCode
{
    ///< No error; the operation completed as requested.
    none,

    ///< A caller-supplied value failed a precondition (e.g. an
    ///< out-of-range timestamp, an invalid `ClickEvent`, or a
    ///< malformed config); the operation did not run.
    validation,

    ///< Reserved for queue-related failures. Note: the current
    ///< `IClickEventQueue`/`BoundedClickEventQueue` contract
    ///< reports overflow via `EnqueueResult::DroppedFull`, not
    ///< via this code - no code in this module currently sets it.
    queue,

    ///< The storage backend rejected or failed the operation
    ///< (`IClickEventRepository::InsertBatch`/`GetAggregateStats`/
    ///< `DeleteOlderThan`); no data was written or read.
    repository,

    ///< The background `AnalyticsWorker` failed to persist a
    ///< batch after exhausting its retry budget.
    worker,

    ///< Reserved for retention/cleanup-specific failures.
    ///< `RetentionCleanupJob::Run` currently surfaces a failed
    ///< delete pass via the `repository` code from
    ///< `IClickEventRepository::DeleteOlderThan`, not this one.
    retention
};

/**
 * @brief Domain error model used by analytics components, filled in on
 * failure via the project's `bool` + `*error` out-param convention (see
 * `CLAUDE.md` "Storage: ports and adapters").
 */
struct AnalyticsError
{
    ///< Failure category; `none` only on a
    ///< default-constructed/untouched instance -
    ///< callers should check the method's `bool`
    ///< return, not this field, to detect
    ///< failure.
    AnalyticsErrorCode code = AnalyticsErrorCode::none;

    ///< Human-readable detail for logs/diagnostics; not
    ///< guaranteed stable across versions and not meant
    ///< to be shown to end users verbatim.
    std::string message;
};

/**
 * @brief Stream formatter for analytics error codes.
 * @param os Stream to write to.
 * @param code Value to format.
 * @return `os`, with the code's symbolic name (e.g.
 * `"AnalyticsErrorCode::validation"`) written to it.
 */
inline std::ostream& operator<<(std::ostream& os, AnalyticsErrorCode code)
{
    switch (code) {
        case AnalyticsErrorCode::none:
            return os << "AnalyticsErrorCode::none";
        case AnalyticsErrorCode::validation:
            return os << "AnalyticsErrorCode::validation";
        case AnalyticsErrorCode::queue:
            return os << "AnalyticsErrorCode::queue";
        case AnalyticsErrorCode::repository:
            return os << "AnalyticsErrorCode::repository";
        case AnalyticsErrorCode::worker:
            return os << "AnalyticsErrorCode::worker";
        case AnalyticsErrorCode::retention:
            return os << "AnalyticsErrorCode::retention";
    }
    return os << "AnalyticsErrorCode(" << static_cast<int>(code) << ")";
}

/**
 * @brief Stream formatter for analytics error values.
 * @param os Stream to write to.
 * @param error Value to format.
 * @return `os`, with `error.code` and `error.message` written to it as
 * `AnalyticsError{code=..., message='...'}`.
 */
inline std::ostream& operator<<(std::ostream& os, const AnalyticsError& error)
{
    return os << "AnalyticsError{code=" << error.code << ", message='"
              << error.message << "'}";
}

}  // namespace url_shortener::analytics
