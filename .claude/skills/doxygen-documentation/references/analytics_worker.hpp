#pragma once

#include <atomic>
#include <thread>

#include "url_shortener/analytics/analytics_config.hpp"
#include "url_shortener/analytics/analytics_metrics.hpp"
#include "url_shortener/analytics/bounded_click_event_queue.hpp"
#include "url_shortener/analytics/i_click_event_repository.hpp"

namespace url_shortener::analytics
{

/**
 * @brief Lifecycle state of an `AnalyticsWorker`'s background thread.
 */
enum class AnalyticsWorkerState
{
    ///< Initial state, and the state after `Stop()` has finished
    ///< joining the background thread. `Start()` may be called
    ///< from this state.
    Stopped,

    ///< `Start()` has spawned the background thread and it is
    ///< actively draining the queue in a loop.
    Running,

    ///< `Stop()` has requested a shutdown and woken the queue via
    ///< `BoundedClickEventQueue::NotifyStop`; the background
    ///< thread is draining any remaining buffered events before
    ///< it exits and the state moves to `Stopped`.
    Stopping
};

/**
 * @brief Background worker that drains `BoundedClickEventQueue` in batches
 * and persists them to `IClickEventRepository`, off the redirect hot path.
 *
 * Lifetime: does not own `config`, `queue`, `repo`, or `metrics`; the
 * caller (composition root) must keep all four alive for at least as long
 * as this object exists - including across `Stop()`, since the destructor
 * also calls `Stop()`. Thread-safety: `Start()`/`Stop()` are not safe to
 * call concurrently with each other or with themselves from multiple
 * threads - the caller must serialize lifecycle calls (e.g. from a single
 * composition-root startup/shutdown sequence). `State()` is safe to call
 * from any thread at any time (backed by an atomic). `FlushOnce()` is
 * normally only invoked internally by the background thread's `Run()`
 * loop, but is also safe to call directly (e.g. from a test) to drain and
 * persist one batch synchronously without starting the background thread.
 */
class AnalyticsWorker
{
public:
    /**
     * @brief Stores the given references and starts in state `Stopped`;
     *
     * does not spawn the background thread.
     * @param config Analytics subsystem configuration; must outlive this
     * object.
     * @param queue Queue this worker drains; must outlive this object.
     * @param repo Repository batches are persisted to; must outlive this
     * object.
     * @param metrics Metrics sink updated on every flush; must outlive
     * this object.
     */
    AnalyticsWorker(const AnalyticsConfig& config,
                    BoundedClickEventQueue& queue,
                    IClickEventRepository& repo,
                    IAnalyticsMetrics& metrics);

    /**
     * @brief Calls `Stop()` (blocking until the background thread, if
     * running, has joined) before destroying this object.
     */
    ~AnalyticsWorker();

    /**
     * @brief Spawns the background thread that runs `Run()` in a loop.
     * A no-op if `config_.enabled` is `false` or the worker is already in
     * state `Running`.
     */
    void Start();

    /**
     * @brief Requests the background thread stop, wakes it via
     * `BoundedClickEventQueue::NotifyStop` so it does not block
     * indefinitely in `WaitDequeue`, and blocks the calling thread until
     * it has joined. A no-op if the worker is not in state `Running`.
     */
    void Stop();

    /**
     * @brief Returns the worker's current lifecycle state.
     * @return The state as of the atomic load; may be stale by the time
     * the caller acts on it if another thread concurrently calls
     * `Start()`/`Stop()`.
     */
    AnalyticsWorkerState State() const noexcept;

    /**
     * @brief Drains up to `config_.batch_size` events from the queue
     * (blocking up to `config_.flush_interval` total via `WaitDequeue`,
     * then draining any remainder already buffered via `TryDequeue` once
     * that deadline has passed), then persists the batch via
     * `PersistWithRetry` and reports the outcome to `metrics_`
     * (`OnPersisted`/`OnWorkerFailure`) plus the post-drain queue depth
     * (`SetQueueDepth`). A no-op if nothing was dequeued before the
     * deadline.
     */
    void FlushOnce();

private:
    /**
     * @brief Background-thread loop: calls `FlushOnce()` repeatedly while
     * in state `Running` or `Stopping`, and exits once in state `Stopping`
     * with an empty queue (i.e. the final drain after `Stop()` is done).
     */
    void Run();

    /**
     * @brief Persists `batch` to `repo_`, retrying on failure.
     * @param batch Events to persist in one call to
     * `IClickEventRepository::InsertBatch`.
     * @return `true` if `InsertBatch` succeeded on the first attempt or
     * any retry; `false` if it still failed after
     * `config_.retry.max_retry_attempts` retries (sleeping
     * `config_.retry.retry_backoff` between attempts).
     */
    bool PersistWithRetry(const std::vector<ClickEvent>& batch);

    ///< Analytics configuration (enabled flag, batch size, flush interval,
    ///< retry policy). Not owned; must outlive this object.
    const AnalyticsConfig& config_;

    ///< Queue drained by `FlushOnce()`. Not owned; must outlive this
    ///< object.
    BoundedClickEventQueue& queue_;

    ///< Repository each batch is persisted to by `PersistWithRetry()`.
    ///< Not owned; must outlive this object.
    IClickEventRepository& repo_;

    ///< Metrics sink updated after every flush. Not owned; must outlive
    ///< this object.
    IAnalyticsMetrics& metrics_;

    ///< Current lifecycle state. Atomic so `State()` and the background
    ///< thread's `Run()` loop can read it without locking while
    ///< `Start()`/`Stop()` transition it.
    std::atomic<AnalyticsWorkerState> state_ {AnalyticsWorkerState::Stopped};

    ///< Background thread executing `Run()`; joinable only between
    ///< `Start()` and the matching `Stop()`.
    std::thread thread_;
};

}  // namespace url_shortener::analytics
