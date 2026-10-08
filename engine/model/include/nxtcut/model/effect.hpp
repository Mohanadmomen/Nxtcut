#pragma once

#include <nxtcut/core/color.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/property.hpp>

#include <map>
#include <string>
#include <variant>

namespace nxtcut::model {

/**
 * @brief Variant representing a strongly typed animatable parameter of an effect.
 */
using EffectParam =
    std::variant<Property<double>, Property<bool>, Property<std::string>, Property<core::Color>>;

/**
 * @brief Represents an instantiated visual or audio effect applied to a clip.
 *
 * The effect_type field is an opaque identifier (e.g. "builtin.gaussian_blur").
 * Concrete effect definitions and execution logic belong to the effects module (Step 9) and
 * plugins. Parameters are stored in a std::map for deterministic key ordering.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct EffectInstance {
    EffectId id;
    std::string effect_type;
    bool enabled{true};
    std::map<std::string, EffectParam> params;
};

}  // namespace nxtcut::model
