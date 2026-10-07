#pragma once

#include <nxtcut/core/result.hpp>
#include <nxtcut/core/unique_task.hpp>

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace nxtcut::core {

/**
 * @brief Scheduling priority level for tasks submitted to ThreadPool.
 */
enum class TaskPriority : std::uint8_t {
    High,
    Normal,
    Low,
};

/**
 * @brief Fixed-size worker thread pool with prioritized FIFO task queues.
 *
 * Workers drain High priority tasks first, then Normal, then Low. FIFO order is preserved
 * within each priority level. Tasks submitted with Low priority may be starved while High
 * or Normal queues remain busy. shutdown() must NOT be called from a worker thread of this
 * pool (a thread cannot join itself).
 *
 * @note Thread safety: Thread-safe (all public methods except wait_idle and shutdown may be called
 * concurrently from any thread; neither wait_idle nor shutdown may be called from worker threads of this pool).
 */
class ThreadPool {
public:
    struct ConstructToken {
    private:
        friend class ThreadPool;
        explicit ConstructToken() = default;
    };

    explicit ThreadPool(ConstructToken, std::size_t thread_count);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool();

    /**
     * @brief Factory creating a new ThreadPool with the specified worker count.
     *
     * @param thread_count Number of worker threads (must be > 0).
     * @return ThreadPool instance or ErrorCode::InvalidArgument if thread_count is 0.
     * Returns ErrorCode::Internal if a worker thread cannot be started.
     */
    [[nodiscard]] static Result<std::unique_ptr<ThreadPool>> create(std::size_t thread_count);

    /**
     * @brief Returns the recommended default worker count based on hardware concurrency.
     */
    [[nodiscard]] static std::size_t default_thread_count() noexcept;

    /**
     * @brief Submits a callable task with the given priority.
     *
     * Exceptions thrown within the callable are captured and rethrown when calling get()
     * on the returned future. If the pool is shutting down or shut down, returns
     * ErrorCode::Cancelled and the task is not executed.
     *
     * @tparam F Move-constructible callable invocable with no arguments.
     * @param f Callable to execute.
     * @param priority Scheduling priority level.
     * @return std::future holding task result, or ErrorCode::Cancelled on shutdown.
     */
    template <class F>
    requires std::is_invocable_v<std::decay_t<F>> && std::is_move_constructible_v<std::decay_t<F>>
    [[nodiscard]] Result<std::future<std::invoke_result_t<std::decay_t<F>>>> submit(
        F&& f, TaskPriority priority = TaskPriority::Normal) {
        using ReturnT = std::invoke_result_t<std::decay_t<F>>;
        std::packaged_task<ReturnT()> task(std::forward<F>(f));
        auto fut = task.get_future();

        UniqueTask u_task([t = std::move(task)]() mutable {
            t();
        });

        auto status = enqueue_task(std::move(u_task), priority);
        if (!status.has_value()) {
            return tl::unexpected(status.error());
        }
        return fut;
    }

    /**
     * @brief Blocks until all task queues are empty and no worker is actively running a task.
     *
     * @pre Must NOT be called from a worker thread of this pool (deadlock).
     */
    void wait_idle();

    /**
     * @brief Initiates graceful shutdown: rejects new submissions, finishes all queued tasks, and joins workers.
     *
     * Calling shutdown() is idempotent. Destructor calls shutdown automatically.
     * shutdown() must NOT be called from a worker thread of this pool (a thread cannot join itself).
     *
     * @pre Must NOT be called from a worker thread of this pool (deadlock / self-join).
     */
    void shutdown();


    /**
     * @brief Number of worker threads managed by this pool.
     */
    [[nodiscard]] std::size_t thread_count() const noexcept;

    /**
     * @brief Number of tasks currently queued and waiting for worker execution.
     */
    [[nodiscard]] std::size_t pending_count() const;

private:
    [[nodiscard]] Status start_threads();
    [[nodiscard]] Status enqueue_task(UniqueTask task, TaskPriority priority);
    void worker_loop();

    std::size_t thread_count_{0};
    std::vector<std::thread> threads_;

    std::deque<UniqueTask> high_queue_;
    std::deque<UniqueTask> normal_queue_;
    std::deque<UniqueTask> low_queue_;

    mutable std::mutex mutex_;
    std::condition_variable work_cv_;
    std::condition_variable idle_cv_;

    std::mutex join_mutex_;
    std::size_t active_tasks_{0};
    bool shutting_down_{false};
};

}  // namespace nxtcut::core
