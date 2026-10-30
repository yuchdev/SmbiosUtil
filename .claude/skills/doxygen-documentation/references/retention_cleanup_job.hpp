#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

#include "url_shortener/analytics/analytics_config.hpp"
#include "url_shortener/analytics/analytics_metrics.hpp"
#include "url_shortener/analytics/i_click_event_repository.hpp"

namespace url_shortener::analytics
{

/** @brief Result of one `RetentionCleanupJob::Run` call. */
struct RetentionCleanupResult
{
    ///< `false` if `AnalyticsConfig::retention.retention_days <= 0`
    ///< (retention disabled) - in that case
    ///< `cutoff`/`deleted_count`/`error` below are left at their
    ///< defaults and no delete was attempted. `true` otherwise.
    bool enabled = false;

    ///< The computed deletion boundary (`now() - 24h *
    ///< retention_days`), set whenever `enabled` is `true` -
    ///< regardless of whether the delete itself succeeded.
    std::optional<Timestamp> cutoff;

    ///< @note Currently always `0`, even on a successful delete:
    ///< `IClickEventRepository::DeleteOlderThan` does not report how
    ///< many rows it removed, so this field does not yet reflect the
    ///< real delete count despite its name. Flagged as a gap for
    ///< `cpp-expert`/`app-architect`, not fixed here.
    std::uint64_t deleted_count = 0;

    ///< Set only if `enabled` is `true` and
    ///< `IClickEventRepository::DeleteOlderThan` failed; unset
    ///< (`std::nullopt`) on success or when `enabled` is `false`.
    std::optional<AnalyticsError> error;
};

/**
 * @brief Executes retention deletion outside request/redirect hot paths
 * (e.g. from a periodic scheduler in the composition root), deleting click
 * events older than `AnalyticsConfig::retention.retention_days`.
 *
 * Lifetime: does not own `config`, `repo`, or `metrics`; the caller must
 * keep all three alive for at least as long as this object exists.
 */
class RetentionCleanupJob
{
public:
    /**
     * @brief Injectable clock type, so tests can control "now" rather
     * than depending on `std::chrono::system_clock::now()` (the same
     * pattern as `url_shortener::core::ManualClock` elsewhere - see
     * `CLAUDE.md` "Test utilities").
     */
    using Clock = std::function<Timestamp()>;

    /**
     * @brief Stores the given references and clock.
     * @param config Supplies `retention.retention_days`; must outlive
     * this object.
     * @param repo Repository deletions are issued against; must outlive
     * this object.
     * @param metrics Metrics sink updated on a failed delete; must
     * outlive this object.
     * @param now Clock used to compute the deletion cutoff. If empty
     * (the default), `std::chrono::system_clock::now` is used.
     */
    RetentionCleanupJob(const AnalyticsConfig& config,
                        IClickEventRepository& repo,
                        IAnalyticsMetrics& metrics,
                        Clock now = {});

    /**
     * @brief Runs one retention cleanup pass.
     *
     * A no-op (returns with `RetentionCleanupResult::enabled == false`)
     * if `config_.retention.retention_days <= 0`. Otherwise computes the
     * cutoff from `now_()`, calls `repo_.DeleteOlderThan(cutoff)`, and on
     * failure calls `metrics_.OnWorkerFailure()`. Never throws.
     * @return A `RetentionCleanupResult` describing the outcome; see its
     * field docs for exactly what is populated in each case.
     */
    RetentionCleanupResult Run() noexcept;

private:
    ///< Supplies `retention.retention_days`. Not owned; must outlive this
    ///< object.
    const AnalyticsConfig& config_;

    ///< Repository `DeleteOlderThan` is issued against. Not owned; must
    ///< outlive this object.
    IClickEventRepository& repo_;

    ///< Metrics sink notified via `OnWorkerFailure()` when a delete
    ///< fails. Not owned; must outlive this object.
    IAnalyticsMetrics& metrics_;

    ///< Clock used to compute the deletion cutoff; defaults to
    ///< `std::chrono::system_clock::now` when none was injected.
    Clock now_;
};

}  // namespace url_shortener::analytics
