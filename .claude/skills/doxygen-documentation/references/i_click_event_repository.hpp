#pragma once

#include <chrono>
#include <vector>

#include "url_shortener/analytics/aggregate_query.hpp"
#include "url_shortener/analytics/aggregate_stats.hpp"
#include "url_shortener/analytics/click_event.hpp"

namespace url_shortener::analytics
{

/**
 * @brief Storage port for click-event persistence and aggregate reads (see
 * `CLAUDE.md` "Storage: ports and adapters"). Implemented per backend
 * under `src/storage/` (in-memory, SQL); called by `AnalyticsWorker`
 * (writes), the analytics management-plane handlers (aggregate reads), and
 * `RetentionCleanupJob` (deletes). As with every port in this codebase,
 * expected failures are reported via the `bool` + `*error` out-param
 * convention, never by throwing.
 */
class IClickEventRepository
{
public:
    /**
     * @brief Virtual destructor; implementations may be destroyed through
     * a base `IClickEventRepository*`.
     */
    virtual ~IClickEventRepository() = default;

    /**
     * @brief Persists a batch of click events.
     * @param events Events to persist; may be empty.
     * @param[out] error If non-null and the call fails, receives a
     * backend-specific failure reason (observed implementations use
     * `AnalyticsErrorCode::repository`); left untouched on success.
     * @return `true` if the batch was accepted by the backend; `false` if
     * it failed. This interface does not itself guarantee all-or-nothing
     * atomicity across the batch or across backends - consult the
     * specific adapter (e.g. `src/storage/sql/`, `src/storage/memory/`)
     * for its actual partial-failure behavior.
     */
    virtual bool InsertBatch(const std::vector<ClickEvent>& events,
                             AnalyticsError* error = nullptr) = 0;

    /**
     * @brief Computes aggregate statistics for the events matching `query`.
     * @param query Filter/time-range parameters; should already be
     * validated via `AggregateQueryValidator::Validate` - behavior for an
     * unvalidated query (e.g. `from > to`) is backend-defined.
     * @param[out] stats Receives the computed `AggregateStats` on success;
     * left unmodified on failure. Must be non-null.
     * @param[out] error If non-null and the call fails, receives a
     * backend-specific failure reason; left untouched on success.
     * @return `true` if `stats` is non-null and the query executed
     * successfully; `false` otherwise (including a null `stats`).
     */
    virtual bool GetAggregateStats(const AggregateQuery& query,
                                   AggregateStats* stats,
                                   AnalyticsError* error = nullptr) = 0;

    /**
     * @brief Deletes events older than `cutoff`, for retention cleanup.
     * @param cutoff Deletes events whose `occurred_at` is strictly before
     * this timestamp (observed implementations use `occurred_at < cutoff`,
     * i.e. `cutoff` itself is retained).
     * @param[out] error If non-null and the call fails, receives a
     * backend-specific failure reason; left untouched on success.
     * @return `true` if the delete completed (including deleting zero
     * rows); `false` if it failed. This interface does not report how
     * many rows were deleted - see `RetentionCleanupResult::deleted_count`
     * for the current caller-side caveat about count tracking.
     */
    virtual bool DeleteOlderThan(Timestamp cutoff,
                                 AnalyticsError* error = nullptr) = 0;
};

}  // namespace url_shortener::analytics
