#include <nxtcut/core/error.hpp>
#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/keyframes/keyframe.hpp>
#include <nxtcut/keyframes/keyframe_track.hpp>
#include <nxtcut/model/checked_arithmetic.hpp>
#include <nxtcut/model/clip_animation.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nxtcut::model {

namespace {

template <class T>
core::Status shift_property_keyframes(Property<T>& prop, core::Duration offset) {
    if (!prop.is_animated()) {
        return core::Status{};
    }
    const auto* track = prop.keyframes();
    if (track == nullptr || track->empty()) {
        return core::Status{};
    }

    std::vector<keyframes::Keyframe<T>> new_keys;
    new_keys.reserve(track->size());

    for (const auto& k : track->keys()) {
        const auto new_ticks = detail::checked_add(k.time.ticks(), offset.ticks());
        if (!new_ticks.has_value() || *new_ticks < -keyframes::kMaxKeyframeTicks ||
            *new_ticks > keyframes::kMaxKeyframeTicks) {
            return core::make_error(core::ErrorCode::InvalidArgument, "keyframe time out of range");
        }
        new_keys.push_back(keyframes::Keyframe<T>{core::TimePoint::from_ticks(*new_ticks), k.value,
                                                  k.interpolation});
    }

    auto created = keyframes::KeyframeTrack<T>::create(std::move(new_keys));
    if (!created.has_value()) {
        return tl::unexpected(created.error());
    }

    prop.set_keyframes(std::move(created.value()));
    return core::Status{};
}

template <class T>
core::Status scale_property_keyframes(Property<T>& prop, std::int64_t numerator,
                                      std::int64_t denominator) {
    if (!prop.is_animated()) {
        return core::Status{};
    }
    const auto* track = prop.keyframes();
    if (track == nullptr || track->empty()) {
        return core::Status{};
    }

    std::vector<keyframes::Keyframe<T>> new_keys;
    new_keys.reserve(track->size());

    std::optional<std::int64_t> last_tick;

    for (const auto& k : track->keys()) {
        const auto new_time_res =
            core::mul_div(k.time.ticks(), numerator, denominator, core::RoundingMode::Nearest);
        if (!new_time_res.has_value()) {
            return tl::unexpected(new_time_res.error());
        }
        const std::int64_t new_tick = *new_time_res;
        if (new_tick < -keyframes::kMaxKeyframeTicks || new_tick > keyframes::kMaxKeyframeTicks) {
            return core::make_error(core::ErrorCode::InvalidArgument, "keyframe time out of range");
        }
        if (last_tick.has_value() && new_tick <= *last_tick) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "keyframe collision at tick " + std::to_string(new_tick));
        }
        last_tick = new_tick;
        new_keys.push_back(keyframes::Keyframe<T>{core::TimePoint::from_ticks(new_tick), k.value,
                                                  k.interpolation});
    }

    auto created = keyframes::KeyframeTrack<T>::create(std::move(new_keys));
    if (!created.has_value()) {
        const std::string inner_message(created.error().message());
        if (inner_message.find("duplicate") != std::string::npos) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "keyframe collision: " + inner_message);
        }
        return tl::unexpected(created.error());
    }

    prop.set_keyframes(std::move(created.value()));
    return core::Status{};
}

}  // namespace

core::Status shift_keyframes(Clip& clip, core::Duration offset) {
    if (offset.ticks() == 0) {
        return core::Status{};
    }

    core::Status status{};
    for_each_animatable_property(clip, [&](auto& prop) {
        if (!status.has_value()) {
            return;
        }
        auto res = shift_property_keyframes(prop, offset);
        if (!res.has_value()) {
            status = res;
        }
    });

    return status;
}

core::Status scale_keyframes(Clip& clip, std::int64_t numerator, std::int64_t denominator) {
    if (numerator <= 0 || denominator <= 0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "scale ratio numerator and denominator must be positive");
    }
    if (numerator == denominator) {
        return core::Status{};
    }

    core::Status status{};
    for_each_animatable_property(clip, [&](auto& prop) {
        if (!status.has_value()) {
            return;
        }
        auto res = scale_property_keyframes(prop, numerator, denominator);
        if (!res.has_value()) {
            status = res;
        }
    });

    return status;
}

}  // namespace nxtcut::model
