#include <nxtcut/model/effect.hpp>

#include <gtest/gtest.h>

namespace nxtcut::model {
namespace {

TEST(EffectTest, EffectParamVariantHoldTypes) {
    EffectParam p_double = Property<double>(1.5);
    EffectParam p_bool = Property<bool>(true);
    EffectParam p_string = Property<std::string>("blur_mode");
    EffectParam p_color = Property<core::Color>(core::Color{1.0f, 0.0f, 0.0f, 1.0f});

    EXPECT_TRUE(std::holds_alternative<Property<double>>(p_double));
    EXPECT_TRUE(std::holds_alternative<Property<bool>>(p_bool));
    EXPECT_TRUE(std::holds_alternative<Property<std::string>>(p_string));
    EXPECT_TRUE(std::holds_alternative<Property<core::Color>>(p_color));

    EXPECT_DOUBLE_EQ(std::get<Property<double>>(p_double).constant_value(), 1.5);
    EXPECT_TRUE(std::get<Property<bool>>(p_bool).constant_value());
    EXPECT_EQ(std::get<Property<std::string>>(p_string).constant_value(), "blur_mode");
    EXPECT_TRUE(std::get<Property<core::Color>>(p_color).constant_value().approx_equal(
        core::Color{1.0f, 0.0f, 0.0f, 1.0f}));
}

TEST(EffectTest, EffectInstanceDeterministicMapOrdering) {
    EffectInstance effect;
    effect.effect_type = "builtin.gaussian_blur";
    effect.enabled = true;

    effect.params["radius"] = Property<double>(5.0);
    effect.params["horizontal"] = Property<bool>(true);
    effect.params["aspect"] = Property<double>(1.0);

    // std::map keys should iterate in alphabetical order
    auto it = effect.params.begin();
    EXPECT_EQ(it->first, "aspect");
    ++it;
    EXPECT_EQ(it->first, "horizontal");
    ++it;
    EXPECT_EQ(it->first, "radius");
    ++it;
    EXPECT_EQ(it, effect.params.end());
}

}  // namespace
}  // namespace nxtcut::model
