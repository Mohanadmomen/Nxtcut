#include <nxtcut/core/geometry.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace nxtcut::core {
namespace {

constexpr double kEps = 1e-9;

TEST(GeometryTest, FitInsideFillOutsideLetterboxVector1) {
    const SizeD content{640.0, 480.0};
    const SizeD container{1920.0, 1080.0};

    const auto fit_res = fit_inside(content, container);
    ASSERT_TRUE(fit_res.has_value());
    EXPECT_NEAR(fit_res->width, 1440.0, kEps);
    EXPECT_NEAR(fit_res->height, 1080.0, kEps);

    const auto fill_res = fill_outside(content, container);
    ASSERT_TRUE(fill_res.has_value());
    EXPECT_NEAR(fill_res->width, 1920.0, kEps);
    EXPECT_NEAR(fill_res->height, 1440.0, kEps);

    const auto lb_res = letterbox_rect(content, container);
    ASSERT_TRUE(lb_res.has_value());
    EXPECT_NEAR(lb_res->x, 240.0, kEps);
    EXPECT_NEAR(lb_res->y, 0.0, kEps);
    EXPECT_NEAR(lb_res->width, 1440.0, kEps);
    EXPECT_NEAR(lb_res->height, 1080.0, kEps);
}

TEST(GeometryTest, FitInsideFillOutsideLetterboxVector2) {
    const SizeD content{1920.0, 1080.0};
    const SizeD container{1080.0, 1920.0};

    const auto fit_res = fit_inside(content, container);
    ASSERT_TRUE(fit_res.has_value());
    EXPECT_NEAR(fit_res->width, 1080.0, kEps);
    EXPECT_NEAR(fit_res->height, 607.5, kEps);

    const auto lb_res = letterbox_rect(content, container);
    ASSERT_TRUE(lb_res.has_value());
    EXPECT_NEAR(lb_res->x, 0.0, kEps);
    EXPECT_NEAR(lb_res->y, 656.25, kEps);
    EXPECT_NEAR(lb_res->width, 1080.0, kEps);
    EXPECT_NEAR(lb_res->height, 607.5, kEps);

    const auto fill_res = fill_outside(content, container);
    ASSERT_TRUE(fill_res.has_value());
    EXPECT_NEAR(fill_res->width, 3413.3333333333335, kEps);
    EXPECT_NEAR(fill_res->height, 1920.0, kEps);
}

TEST(GeometryTest, SameAspectRatioVector) {
    const SizeD content{100.0, 50.0};
    const SizeD container{200.0, 100.0};

    const auto fit_res = fit_inside(content, container);
    ASSERT_TRUE(fit_res.has_value());
    EXPECT_NEAR(fit_res->width, 200.0, kEps);
    EXPECT_NEAR(fit_res->height, 100.0, kEps);

    const auto lb_res = letterbox_rect(content, container);
    ASSERT_TRUE(lb_res.has_value());
    EXPECT_NEAR(lb_res->x, 0.0, kEps);
    EXPECT_NEAR(lb_res->y, 0.0, kEps);
    EXPECT_NEAR(lb_res->width, 200.0, kEps);
    EXPECT_NEAR(lb_res->height, 100.0, kEps);
}

TEST(GeometryTest, InvalidDimensionsReturnError) {
    constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();

    EXPECT_FALSE(fit_inside({0.0, 100.0}, {200.0, 100.0}).has_value());
    EXPECT_FALSE(fit_inside({100.0, -10.0}, {200.0, 100.0}).has_value());
    EXPECT_FALSE(fit_inside({kNan, 100.0}, {200.0, 100.0}).has_value());
    EXPECT_FALSE(fit_inside({100.0, 100.0}, {kInf, 100.0}).has_value());

    EXPECT_FALSE(fill_outside({0.0, 100.0}, {200.0, 100.0}).has_value());
    EXPECT_FALSE(fill_outside({100.0, -10.0}, {200.0, 100.0}).has_value());
    EXPECT_FALSE(fill_outside({kNan, 100.0}, {200.0, 100.0}).has_value());
    EXPECT_FALSE(fill_outside({100.0, 100.0}, {kInf, 100.0}).has_value());

    EXPECT_FALSE(letterbox_rect({0.0, 100.0}, {200.0, 100.0}).has_value());
}

TEST(GeometryTest, RectHalfOpenContains) {
    const RectI r{10, 20, 30, 40};  // [10, 40) x [20, 60)

    EXPECT_TRUE(r.contains(PointI{10, 20}));
    EXPECT_TRUE(r.contains(PointI{39, 59}));

    // Right and bottom edges are excluded in half-open intervals
    EXPECT_FALSE(r.contains(PointI{40, 30}));
    EXPECT_FALSE(r.contains(PointI{20, 60}));
    EXPECT_FALSE(r.contains(PointI{9, 20}));
}

TEST(GeometryTest, RectIntersectsAndTouchingEdges) {
    const RectI r1{0, 0, 10, 10};
    const RectI r2{5, 5, 10, 10};
    EXPECT_TRUE(r1.intersects(r2));

    // Touching edge does not intersect
    const RectI r_touching{10, 0, 10, 10};
    EXPECT_FALSE(r1.intersects(r_touching));

    // Empty rect never intersects
    const RectI r_empty{2, 2, 0, 10};
    EXPECT_FALSE(r1.intersects(r_empty));
}

TEST(GeometryTest, RectIntersectionAndUnited) {
    const RectI r1{0, 0, 10, 10};
    const RectI r2{5, 5, 10, 10};

    const auto inter = r1.intersection(r2);
    ASSERT_TRUE(inter.has_value());
    EXPECT_EQ(*inter, (RectI{5, 5, 5, 5}));

    const auto u = r1.united(r2);
    EXPECT_EQ(u, (RectI{0, 0, 15, 15}));

    const RectI r_empty{};
    EXPECT_EQ(r1.united(r_empty), r1);
    EXPECT_EQ(r_empty.united(r_empty), r_empty);
}

TEST(GeometryTest, RectCenter) {
    const RectI ri{0, 0, 10, 20};
    EXPECT_EQ(ri.center(), (PointI{5, 10}));

    const RectD rd{1.0, 2.0, 10.0, 20.0};
    const PointD c = rd.center();
    EXPECT_NEAR(c.x, 6.0, kEps);
    EXPECT_NEAR(c.y, 12.0, kEps);
}

}  // namespace
}  // namespace nxtcut::core
