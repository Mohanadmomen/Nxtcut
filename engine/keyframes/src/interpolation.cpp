#include <nxtcut/keyframes/interpolation.hpp>

#include <bit>
#include <cmath>

namespace nxtcut::keyframes {

core::Result<Interpolation> Interpolation::bezier(double x1, double y1, double x2, double y2) {
    if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2)) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cubic bezier control points must be finite");
    }
    if (x1 < 0.0 || x1 > 1.0 || x2 < 0.0 || x2 > 1.0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cubic bezier x control points must be in [0, 1]");
    }

    Interpolation result;
    result.kind_ = InterpolationKind::Bezier;
    result.handles_ = BezierHandles{x1, y1, x2, y2};

    // Precompute UnitBezier polynomial coefficients:
    // x(s) = cx*s + bx*s^2 + ax*s^3
    // y(s) = cy*s + by*s^2 + ay*s^3
    result.cx_ = 3.0 * x1;
    result.bx_ = 3.0 * (x2 - x1) - result.cx_;
    result.ax_ = 1.0 - result.cx_ - result.bx_;

    result.cy_ = 3.0 * y1;
    result.by_ = 3.0 * (y2 - y1) - result.cy_;
    result.ay_ = 1.0 - result.cy_ - result.by_;

    return result;
}

double Interpolation::map_progress_slow(double t) const noexcept {
    if (kind_ == InterpolationKind::Easing) {
        return ease(easing_kind_, t);
    }
    if (kind_ == InterpolationKind::Bezier) {
        // Fast path for identity curve
        if (handles_.x1 == handles_.y1 && handles_.x2 == handles_.y2) {
            return t;
        }
        if (t <= 0.0) {
            return 0.0;
        }
        if (t >= 1.0) {
            return 1.0;
        }

        // Solve x(s) = t using Newton-Raphson starting at s = t
        double s = t;
        bool converged = false;

        for (int iter = 0; iter < 8; ++iter) {
            const double x_curr = ((ax_ * s + bx_) * s + cx_) * s;
            const double x_err = x_curr - t;
            if (std::abs(x_err) < 1e-12) {
                converged = true;
                break;
            }
            const double d = (3.0 * ax_ * s + 2.0 * bx_) * s + cx_;
            if (std::abs(d) < 1e-6) {
                break;
            }
            const double s_next = s - x_err / d;
            if (s_next < 0.0 || s_next > 1.0) {
                break;
            }
            s = s_next;
        }

        if (!converged) {
            // Fall back to bisection on [0, 1] with at most 64 iterations
            double s_low = 0.0;
            double s_high = 1.0;
            for (int iter = 0; iter < 64; ++iter) {
                s = (s_low + s_high) * 0.5;
                const double x_curr = ((ax_ * s + bx_) * s + cx_) * s;
                const double x_err = x_curr - t;
                if (std::abs(x_err) < 1e-12) {
                    break;
                }
                if (x_err > 0.0) {
                    s_high = s;
                } else {
                    s_low = s;
                }
            }
        }

        return ((ay_ * s + by_) * s + cy_) * s;
    }
    return t;
}

namespace {

[[nodiscard]] bool bit_identical_double(double a, double b) noexcept {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

}  // namespace

bool identical(const Interpolation& a, const Interpolation& b) noexcept {
    if (a.kind() != b.kind()) {
        return false;
    }
    if (a.kind() == InterpolationKind::Easing) {
        return a.easing_kind() == b.easing_kind();
    }
    if (a.kind() == InterpolationKind::Bezier) {
        const auto ha = a.handles();
        const auto hb = b.handles();
        return bit_identical_double(ha.x1, hb.x1) && bit_identical_double(ha.y1, hb.y1) &&
               bit_identical_double(ha.x2, hb.x2) && bit_identical_double(ha.y2, hb.y2);
    }
    return true;
}

}  // namespace nxtcut::keyframes
