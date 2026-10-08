#pragma once

#include <nxtcut/model/time_coords.hpp>

#include <utility>

namespace nxtcut::model {

/**
 * @brief Represents an animatable value parameter.
 *
 * Currently holds a single constant value. Step 4 will add keyframe storage
 * behind this same interface.
 *
 * @tparam T The parameter payload value type (e.g. double, bool, std::string, core::Color).
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
template <class T>
class Property {
public:
    Property() : constant_{} {}

    explicit Property(T constant) : constant_(std::move(constant)) {}

    [[nodiscard]] const T& constant_value() const noexcept { return constant_; }

    void set_constant(T constant) { constant_ = std::move(constant); }

    [[nodiscard]] bool is_animated() const noexcept { return false; }

    [[nodiscard]] T value_at(ClipTime /*time*/) const { return constant_; }

private:
    T constant_{};
};

}  // namespace nxtcut::model
