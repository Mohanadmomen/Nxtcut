#include <nxtcut/core/thread_pool.hpp>

#include <algorithm>
#include <memory>
#include <system_error>
#include <utility>

namespace nxtcut::core {

ThreadPool::ThreadPool(ConstructToken, std::size_t thread_count) : thread_count_(thread_count) {}

ThreadPool::~ThreadPool() {
    shutdown();
}

Result<std::unique_ptr<ThreadPool>> ThreadPool::create(std::size_t thread_count) {
    if (thread_count == 0) {
        return make_error(ErrorCode::InvalidArgument, "thread count must be greater than zero");
    }

    auto pool = std::make_unique<ThreadPool>(ConstructToken{}, thread_count);
    const Status status = pool->start_threads();
    if (!status.has_value()) {
        return tl::unexpected(status.error());
    }
    return pool;
}

std::size_t ThreadPool::default_thread_count() noexcept {
    const unsigned int count = std::thread::hardware_concurrency();
    return count > 0 ? static_cast<std::size_t>(count) : 1U;
}

Status ThreadPool::start_threads() {
    try {
        threads_.reserve(thread_count_);
        for (std::size_t i = 0; i < thread_count_; ++i) {
            threads_.emplace_back([this] { worker_loop(); });
        }
    } catch (const std::system_error& ex) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutting_down_ = true;
            work_cv_.notify_all();
        }
        for (auto& th : threads_) {
            if (th.joinable()) {
                th.join();
            }
        }
        threads_.clear();
        return make_error(ErrorCode::Internal, ex.what());
    }
    return Status{};
}

Status ThreadPool::enqueue_task(UniqueTask task, TaskPriority priority) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (shutting_down_) {
        return make_error(ErrorCode::Cancelled, "thread pool is shut down");
    }

    switch (priority) {
        case TaskPriority::High:
            high_queue_.push_back(std::move(task));
            break;
        case TaskPriority::Normal:
            normal_queue_.push_back(std::move(task));
            break;
        case TaskPriority::Low:
            low_queue_.push_back(std::move(task));
            break;
    }

    work_cv_.notify_one();
    return Status{};
}

void ThreadPool::worker_loop() {
    while (true) {
        UniqueTask task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_cv_.wait(lock, [this] {
                return shutting_down_ || !high_queue_.empty() || !normal_queue_.empty() ||
                       !low_queue_.empty();
            });

            if (!high_queue_.empty()) {
                task = std::move(high_queue_.front());
                high_queue_.pop_front();
            } else if (!normal_queue_.empty()) {
                task = std::move(normal_queue_.front());
                normal_queue_.pop_front();
            } else if (!low_queue_.empty()) {
                task = std::move(low_queue_.front());
                low_queue_.pop_front();
            } else if (shutting_down_) {
                // All queues are drained and pool is shutting down: worker exits
                break;
            }

            if (task) {
                ++active_tasks_;
            }
        }

        if (task) {
            task();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                --active_tasks_;
                if (active_tasks_ == 0 && high_queue_.empty() && normal_queue_.empty() &&
                    low_queue_.empty()) {
                    idle_cv_.notify_all();
                }
            }
        }
    }
}

void ThreadPool::wait_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_cv_.wait(lock, [this] {
        return active_tasks_ == 0 && high_queue_.empty() && normal_queue_.empty() &&
               low_queue_.empty();
    });
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!shutting_down_) {
            shutting_down_ = true;
            work_cv_.notify_all();
        }
    }

    std::lock_guard<std::mutex> join_lock(join_mutex_);
    for (auto& worker : threads_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        idle_cv_.notify_all();
    }
}

std::size_t ThreadPool::thread_count() const noexcept {
    return thread_count_;
}

std::size_t ThreadPool::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return high_queue_.size() + normal_queue_.size() + low_queue_.size();
}

}  // namespace nxtcut::core
