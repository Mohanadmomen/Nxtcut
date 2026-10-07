#include <nxtcut/core/affine_transform.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nxtcut::core {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

TEST(AffineTransformTest, RequiredVectorRotations) {
    const auto r_half_pi = AffineTransform::rotation(kPi / 2.0);
    const PointD p1 = r_half_pi.apply(PointD{1.0, 0.0});
    EXPECT_NEAR(p1.x, 0.0, kEps);
    EXPECT_NEAR(p1.y, 1.0, kEps);

    const auto r_pi = AffineTransform::rotation(kPi);
    const PointD p2 = r_pi.apply(PointD{1.0, 0.0});
    EXPECT_NEAR(p2.x, -1.0, kEps);
    EXPECT_NEAR(p2.y, 0.0, kEps);
}

TEST(AffineTransformTest, RequiredVectorCompositionOrder) {
    const auto t = AffineTransform::translation(10.0, 20.0);
    const auto s = AffineTransform::scale(2.0, 3.0);

    // t.then(s) applies t first, then s
    const PointD res1 = t.then(s).apply(PointD{1.0, 1.0});
    EXPECT_NEAR(res1.x, 22.0, kEps);
    EXPECT_NEAR(res1.y, 63.0, kEps);

    // s.then(t) applies s first, then t
    const PointD res2 = s.then(t).apply(PointD{1.0, 1.0});
    EXPECT_NEAR(res2.x, 12.0, kEps);
    EXPECT_NEAR(res2.y, 23.0, kEps);
}

TEST(AffineTransformTest, RequiredVectorTransformRect) {
    const auto r_half_pi = AffineTransform::rotation(kPi / 2.0);
    const RectD transformed = r_half_pi.apply(RectD{0.0, 0.0, 2.0, 1.0});

    EXPECT_NEAR(transformed.x, -1.0, kEps);
    EXPECT_NEAR(transformed.y, 0.0, kEps);
    EXPECT_NEAR(transformed.width, 1.0, kEps);
    EXPECT_NEAR(transformed.height, 2.0, kEps);
}

TEST(AffineTransformTest, RequiredVectorInverseRoundTrip) {
    const auto t = AffineTransform::translation(10.0, 20.0)
                       .then(AffineTransform::rotation(0.3))
                       .then(AffineTransform::scale(2.0, 3.0));

    const auto inv_res = t.inverse();
    ASSERT_TRUE(inv_res.has_value());
    const AffineTransform inv = *inv_res;

    const std::vector<PointD> points{
        {0.0, 0.0},
        {1.0, 0.0},
        {0.0, 1.0},
        {-5.0, 7.5},
    };

    for (const auto& p : points) {
        const PointD roundtrip = inv.apply(t.apply(p));
        EXPECT_NEAR(roundtrip.x, p.x, kEps);
        EXPECT_NEAR(roundtrip.y, p.y, kEps);
    }
}

TEST(AffineTransformTest, RequiredVectorInvertibilityAndIdentity) {
    // Singular matrix: determinant is 0
    const auto non_invertible = AffineTransform::scale(0.0, 1.0);
    const auto inv_err = non_invertible.inverse();
    ASSERT_FALSE(inv_err.has_value());
    EXPECT_EQ(inv_err.error().code(), ErrorCode::InvalidArgument);

    // Identity inverse is identity
    const auto id = AffineTransform::identity();
    const auto id_inv_res = id.inverse();
    ASSERT_TRUE(id_inv_res.has_value());
    EXPECT_TRUE(id_inv_res->approx_equal(id, kEps));
}

TEST(AffineTransformTest, RequiredVectorAssociativity) {
    const auto a = AffineTransform::translation(5.0, -3.0);
    const auto b = AffineTransform::rotation(0.7);
    const auto c = AffineTransform::scale(1.5, 0.8);

    const auto combo1 = (a.then(b)).then(c);
    const auto combo2 = a.then(b.then(c));

    const std::vector<PointD> test_points{
        {0.0, 0.0},
        {10.0, -4.0},
        {-3.5, 8.2},
    };

    for (const auto& p : test_points) {
        const PointD p1 = combo1.apply(p);
        const PointD p2 = combo2.apply(p);
        EXPECT_NEAR(p1.x, p2.x, kEps);
        EXPECT_NEAR(p1.y, p2.y, kEps);
    }
}

}  // namespace
}  // namespace nxtcut::core
