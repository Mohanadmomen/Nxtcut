#include <nxtcut/core/color.hpp>
#include <nxtcut/model/property.hpp>

#include <gtest/gtest.h>

#include <string>

namespace nxtcut::model {
namespace {

TEST(PropertyTest, DoubleProperty) {
    Property<double> prop_default;
    EXPECT_DOUBLE_EQ(prop_default.constant_value(), 0.0);
    EXPECT_FALSE(prop_default.is_animated());
    EXPECT_DOUBLE_EQ(prop_default.value_at(ClipTime::from_ticks(100)), 0.0);

    Property<double> prop_init(3.14);
    EXPECT_DOUBLE_EQ(prop_init.constant_value(), 3.14);
    EXPECT_DOUBLE_EQ(prop_init.value_at(ClipTime::from_ticks(500)), 3.14);

    prop_init.set_constant(2.718);
    EXPECT_DOUBLE_EQ(prop_init.constant_value(), 2.718);
    EXPECT_DOUBLE_EQ(prop_init.value_at(ClipTime::zero()), 2.718);
}

TEST(PropertyTest, BoolProperty) {
    Property<bool> flag(true);
    EXPECT_TRUE(flag.constant_value());
    EXPECT_FALSE(flag.is_animated());
    EXPECT_TRUE(flag.value_at(ClipTime::from_ticks(10)));

    flag.set_constant(false);
    EXPECT_FALSE(flag.constant_value());
    EXPECT_FALSE(flag.value_at(ClipTime::from_ticks(20)));
}

TEST(PropertyTest, StringProperty) {
    Property<std::string> text("hello");
    EXPECT_EQ(text.constant_value(), "hello");
    EXPECT_EQ(text.value_at(ClipTime::from_ticks(0)), "hello");

    text.set_constant("world");
    EXPECT_EQ(text.constant_value(), "world");
    EXPECT_EQ(text.value_at(ClipTime::from_ticks(99)), "world");
}

TEST(PropertyTest, ColorProperty) {
    const core::Color red{1.0f, 0.0f, 0.0f, 1.0f};
    const core::Color blue{0.0f, 0.0f, 1.0f, 1.0f};

    Property<core::Color> color_prop(red);
    EXPECT_TRUE(color_prop.constant_value().approx_equal(red));
    EXPECT_TRUE(color_prop.value_at(ClipTime::from_ticks(42)).approx_equal(red));

    color_prop.set_constant(blue);
    EXPECT_TRUE(color_prop.constant_value().approx_equal(blue));
    EXPECT_TRUE(color_prop.value_at(ClipTime::from_ticks(100)).approx_equal(blue));
}

TEST(PropertyTest, CopyIndependence) {
    Property<double> original(10.0);
    Property<double> copy = original;

    EXPECT_DOUBLE_EQ(copy.constant_value(), 10.0);

    copy.set_constant(99.0);
    EXPECT_DOUBLE_EQ(copy.constant_value(), 99.0);
    EXPECT_DOUBLE_EQ(original.constant_value(), 10.0);
}

}  // namespace
}  // namespace nxtcut::model
