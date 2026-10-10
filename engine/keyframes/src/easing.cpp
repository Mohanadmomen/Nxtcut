#include <nxtcut/keyframes/easing.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nxtcut::keyframes {

namespace {

inline constexpr double kPi = std::numbers::pi_v<double>;

}  // namespace

double ease(EasingKind kind, double t) noexcept {
    if (std::isnan(t) || t <= 0.0) {
        return 0.0;
    }
    if (t >= 1.0) {
        return 1.0;
    }

    switch (kind) {
        case EasingKind::Linear:
            return t;

        case EasingKind::EaseInSine:
            return 1.0 - std::cos((t * kPi) / 2.0);
        case EasingKind::EaseOutSine:
            return std::sin((t * kPi) / 2.0);
        case EasingKind::EaseInOutSine:
            return -(std::cos(kPi * t) - 1.0) / 2.0;

        case EasingKind::EaseInQuad:
            return t * t;
        case EasingKind::EaseOutQuad: {
            const double inv = 1.0 - t;
            return 1.0 - inv * inv;
        }
        case EasingKind::EaseInOutQuad:
            if (t < 0.5) {
                return 2.0 * t * t;
            } else {
                const double inv = 1.0 - t;
                return 1.0 - 2.0 * inv * inv;
            }

        case EasingKind::EaseInCubic:
            return t * t * t;
        case EasingKind::EaseOutCubic: {
            const double inv = 1.0 - t;
            return 1.0 - inv * inv * inv;
        }
        case EasingKind::EaseInOutCubic:
            if (t < 0.5) {
                return 4.0 * t * t * t;
            } else {
                const double inv = 1.0 - t;
                return 1.0 - 4.0 * inv * inv * inv;
            }

        case EasingKind::EaseInQuart:
            return t * t * t * t;
        case EasingKind::EaseOutQuart: {
            const double inv = 1.0 - t;
            return 1.0 - inv * inv * inv * inv;
        }
        case EasingKind::EaseInOutQuart:
            if (t < 0.5) {
                return 8.0 * t * t * t * t;
            } else {
                const double inv = 1.0 - t;
                return 1.0 - 8.0 * inv * inv * inv * inv;
            }

        case EasingKind::EaseInExpo:
            return std::exp2(10.0 * (t - 1.0));
        case EasingKind::EaseOutExpo:
            return 1.0 - std::exp2(-10.0 * t);
        case EasingKind::EaseInOutExpo:
            if (t < 0.5) {
                return std::exp2(20.0 * t - 10.0) / 2.0;
            } else {
                return (2.0 - std::exp2(-20.0 * t + 10.0)) / 2.0;
            }

        case EasingKind::EaseInCirc: {
            const double d = std::max(0.0, 1.0 - t * t);
            return 1.0 - std::sqrt(d);
        }
        case EasingKind::EaseOutCirc: {
            const double inv = 1.0 - t;
            const double d = std::max(0.0, 1.0 - inv * inv);
            return std::sqrt(d);
        }
        case EasingKind::EaseInOutCirc:
            if (t < 0.5) {
                const double d = std::max(0.0, 1.0 - 4.0 * t * t);
                return (1.0 - std::sqrt(d)) / 2.0;
            } else {
                const double inv = 1.0 - t;
                const double d = std::max(0.0, 1.0 - 4.0 * inv * inv);
                return (std::sqrt(d) + 1.0) / 2.0;
            }
    }

    return t;
}

std::string_view to_string(EasingKind kind) noexcept {
    switch (kind) {
        case EasingKind::Linear:
            return "linear";
        case EasingKind::EaseInSine:
            return "ease-in-sine";
        case EasingKind::EaseOutSine:
            return "ease-out-sine";
        case EasingKind::EaseInOutSine:
            return "ease-in-out-sine";
        case EasingKind::EaseInQuad:
            return "ease-in-quad";
        case EasingKind::EaseOutQuad:
            return "ease-out-quad";
        case EasingKind::EaseInOutQuad:
            return "ease-in-out-quad";
        case EasingKind::EaseInCubic:
            return "ease-in-cubic";
        case EasingKind::EaseOutCubic:
            return "ease-out-cubic";
        case EasingKind::EaseInOutCubic:
            return "ease-in-out-cubic";
        case EasingKind::EaseInQuart:
            return "ease-in-quart";
        case EasingKind::EaseOutQuart:
            return "ease-out-quart";
        case EasingKind::EaseInOutQuart:
            return "ease-in-out-quart";
        case EasingKind::EaseInExpo:
            return "ease-in-expo";
        case EasingKind::EaseOutExpo:
            return "ease-out-expo";
        case EasingKind::EaseInOutExpo:
            return "ease-in-out-expo";
        case EasingKind::EaseInCirc:
            return "ease-in-circ";
        case EasingKind::EaseOutCirc:
            return "ease-out-circ";
        case EasingKind::EaseInOutCirc:
            return "ease-in-out-circ";
    }
    return "linear";
}

std::optional<EasingKind> easing_from_string(std::string_view str) noexcept {
    if (str == "linear")
        return EasingKind::Linear;
    if (str == "ease-in-sine")
        return EasingKind::EaseInSine;
    if (str == "ease-out-sine")
        return EasingKind::EaseOutSine;
    if (str == "ease-in-out-sine")
        return EasingKind::EaseInOutSine;
    if (str == "ease-in-quad")
        return EasingKind::EaseInQuad;
    if (str == "ease-out-quad")
        return EasingKind::EaseOutQuad;
    if (str == "ease-in-out-quad")
        return EasingKind::EaseInOutQuad;
    if (str == "ease-in-cubic")
        return EasingKind::EaseInCubic;
    if (str == "ease-out-cubic")
        return EasingKind::EaseOutCubic;
    if (str == "ease-in-out-cubic")
        return EasingKind::EaseInOutCubic;
    if (str == "ease-in-quart")
        return EasingKind::EaseInQuart;
    if (str == "ease-out-quart")
        return EasingKind::EaseOutQuart;
    if (str == "ease-in-out-quart")
        return EasingKind::EaseInOutQuart;
    if (str == "ease-in-expo")
        return EasingKind::EaseInExpo;
    if (str == "ease-out-expo")
        return EasingKind::EaseOutExpo;
    if (str == "ease-in-out-expo")
        return EasingKind::EaseInOutExpo;
    if (str == "ease-in-circ")
        return EasingKind::EaseInCirc;
    if (str == "ease-out-circ")
        return EasingKind::EaseOutCirc;
    if (str == "ease-in-out-circ")
        return EasingKind::EaseInOutCirc;
    return std::nullopt;
}

}  // namespace nxtcut::keyframes
