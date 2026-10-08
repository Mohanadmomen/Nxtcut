#include <nxtcut/core/affine_transform.hpp>

#include <algorithm>
#include <cmath>

namespace nxtcut::core {

AffineTransform AffineTransform::rotation(double radians) noexcept {
    const double cos_v = std::cos(radians);
    const double sin_v = std::sin(radians);
    return from_matrix(cos_v, sin_v, -sin_v, cos_v, 0.0, 0.0);
}

AffineTransform AffineTransform::then(const AffineTransform& next) const noexcept {
    return from_matrix(next.a_ * a_ + next.c_ * b_, next.b_ * a_ + next.d_ * b_,
                       next.a_ * c_ + next.c_ * d_, next.b_ * c_ + next.d_ * d_,
                       next.a_ * tx_ + next.c_ * ty_ + next.tx_,
                       next.b_ * tx_ + next.d_ * ty_ + next.ty_);
}

PointD AffineTransform::apply(PointD p) const noexcept {
    return PointD{
        a_ * p.x + c_ * p.y + tx_,
        b_ * p.x + d_ * p.y + ty_,
    };
}

RectD AffineTransform::apply(RectD rect) const noexcept {
    const PointD c0{rect.left(), rect.top()};
    const PointD c1{rect.right(), rect.top()};
    const PointD c2{rect.right(), rect.bottom()};
    const PointD c3{rect.left(), rect.bottom()};

    const PointD p0 = apply(c0);
    const PointD p1 = apply(c1);
    const PointD p2 = apply(c2);
    const PointD p3 = apply(c3);

    const double min_x = std::min({p0.x, p1.x, p2.x, p3.x});
    const double max_x = std::max({p0.x, p1.x, p2.x, p3.x});
    const double min_y = std::min({p0.y, p1.y, p2.y, p3.y});
    const double max_y = std::max({p0.y, p1.y, p2.y, p3.y});

    return RectD{min_x, min_y, max_x - min_x, max_y - min_y};
}

Result<AffineTransform> AffineTransform::inverse() const {
    if (!std::isfinite(a_) || !std::isfinite(b_) || !std::isfinite(c_) || !std::isfinite(d_) ||
        !std::isfinite(tx_) || !std::isfinite(ty_)) {
        return make_error(ErrorCode::InvalidArgument, "matrix elements must be finite");
    }

    const double det = determinant();
    if (!std::isfinite(det) || std::abs(det) <= 1e-12) {
        return make_error(ErrorCode::InvalidArgument, "matrix is not invertible");
    }

    const double inv_det = 1.0 / det;
    const double inv_a = d_ * inv_det;
    const double inv_b = -b_ * inv_det;
    const double inv_c = -c_ * inv_det;
    const double inv_d = a_ * inv_det;
    const double inv_tx = (c_ * ty_ - d_ * tx_) * inv_det;
    const double inv_ty = (b_ * tx_ - a_ * ty_) * inv_det;

    return from_matrix(inv_a, inv_b, inv_c, inv_d, inv_tx, inv_ty);
}

bool AffineTransform::approx_equal(const AffineTransform& other, double epsilon) const noexcept {
    return std::abs(a_ - other.a_) <= epsilon && std::abs(b_ - other.b_) <= epsilon &&
           std::abs(c_ - other.c_) <= epsilon && std::abs(d_ - other.d_) <= epsilon &&
           std::abs(tx_ - other.tx_) <= epsilon && std::abs(ty_ - other.ty_) <= epsilon;
}

}  // namespace nxtcut::core
