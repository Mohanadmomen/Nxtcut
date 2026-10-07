#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace nxtcut::core {

/**
 * @brief Move-only, type-erased callable wrapper for invocable objects taking zero arguments.
 *
 * Unlike std::function, UniqueTask can store move-only callables such as std::packaged_task
 * or lambdas capturing std::unique_ptr. Calling an empty task is a safe, documented no-op.
 *
 * @note Thread safety: Move-only value semantics (distinct instances are thread-safe; concurrent access to the same instance requires external synchronization).
 */
class UniqueTask {
    struct Concept {
        virtual ~Concept() = default;
        virtual void call() = 0;
    };

    template <class F>
    struct Model final : Concept {
        F func_;
        explicit Model(F&& f) : func_(std::move(f)) {}
        void call() override { func_(); }
    };

public:
    /**
     * @brief Constructs an empty task. Calling it is a no-op.
     */
    UniqueTask() noexcept = default;

    /**
     * @brief Constructs a UniqueTask wrapping any move-constructible callable invocable with no arguments.
     */
    template <class F>
    requires (!std::same_as<std::remove_cvref_t<F>, UniqueTask> &&
              std::is_move_constructible_v<std::decay_t<F>> &&
              std::is_invocable_r_v<void, std::decay_t<F>>)
    UniqueTask(F&& f)
        : impl_(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(f))) {}

    UniqueTask(const UniqueTask&) = delete;
    UniqueTask& operator=(const UniqueTask&) = delete;

    UniqueTask(UniqueTask&&) noexcept = default;
    UniqueTask& operator=(UniqueTask&&) noexcept = default;

    ~UniqueTask() = default;

    /**
     * @brief Checks whether this task contains a valid callable.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return impl_ != nullptr;
    }

    /**
     * @brief Invokes the stored callable. Calling an empty task is a documented no-op.
     */
    void operator()() {
        if (impl_) {
            impl_->call();
        }
    }

private:
    std::unique_ptr<Concept> impl_;
};

}  // namespace nxtcut::core
