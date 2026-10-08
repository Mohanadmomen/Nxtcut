#pragma once

#include <nxtcut/core/result.hpp>

#include <atomic>
#include <memory>

namespace nxtcut::core {

class CancellationSource;

/**
 * @brief Cheap, copyable token representing a cancellation observation point.
 *
 * A default-constructed token is never cancelled and remains valid indefinitely.
 * Tokens safely outlive their originating CancellationSource by holding shared state.
 *
 * @note Thread safety: Thread-safe (atomic load with acquire memory ordering).
 */
class CancellationToken {
public:
    /**
     * @brief Constructs a valid token that is never cancelled.
     */
    CancellationToken() noexcept = default;

    /**
     * @brief Checks whether cancellation has been requested.
     */
    [[nodiscard]] bool is_cancelled() const noexcept;

    /**
     * @brief Verifies that cancellation has not been requested.
     *
     * @return Success Status if not cancelled, or ErrorCode::Cancelled with message "operation
     * cancelled".
     */
    [[nodiscard]] Status check() const;

private:
    friend class CancellationSource;
    explicit CancellationToken(std::shared_ptr<const std::atomic<bool>> state) noexcept
        : state_(std::move(state)) {}

    std::shared_ptr<const std::atomic<bool>> state_;
};

/**
 * @brief Controlling source that initiates cancellation across associated CancellationToken
 * instances.
 *
 * Non-copyable and non-movable: a CancellationSource represents an authoritative cancellation
 * domain lifecycle. Disallowing copy and move guarantees that the source's identity and ownership
 * boundary remain uniquely anchored and cannot be transferred during concurrent execution.
 * Cancellation is one-shot; there is no reset.
 *
 * @note Thread safety: Thread-safe (atomic store with release memory ordering).
 */
class CancellationSource {
public:
    CancellationSource();

    CancellationSource(const CancellationSource&) = delete;
    CancellationSource& operator=(const CancellationSource&) = delete;
    CancellationSource(CancellationSource&&) = delete;
    CancellationSource& operator=(CancellationSource&&) = delete;
    ~CancellationSource() = default;

    /**
     * @brief Requests cancellation across all associated tokens (idempotent).
     *
     * Memory ordering: stores true with std::memory_order_release.
     */
    void request_cancel() noexcept;

    /**
     * @brief Checks whether cancellation has been requested on this source.
     *
     * Memory ordering: loads with std::memory_order_acquire.
     */
    [[nodiscard]] bool is_cancelled() const noexcept;

    /**
     * @brief Issues a CancellationToken linked to this source.
     */
    [[nodiscard]] CancellationToken token() const;

private:
    std::shared_ptr<std::atomic<bool>> state_;
};

}  // namespace nxtcut::core
