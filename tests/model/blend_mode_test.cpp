#include <nxtcut/model/blend_mode.hpp>

#include <gtest/gtest.h>

namespace nxtcut::model {
namespace {

TEST(BlendModeTest, ToStringCoverage) {
    EXPECT_EQ(to_string(BlendMode::Normal), "Normal");
    EXPECT_EQ(to_string(BlendMode::Multiply), "Multiply");
    EXPECT_EQ(to_string(BlendMode::Screen), "Screen");
    EXPECT_EQ(to_string(BlendMode::Overlay), "Overlay");
    EXPECT_EQ(to_string(BlendMode::Darken), "Darken");
    EXPECT_EQ(to_string(BlendMode::Lighten), "Lighten");
    EXPECT_EQ(to_string(BlendMode::ColorDodge), "ColorDodge");
    EXPECT_EQ(to_string(BlendMode::ColorBurn), "ColorBurn");
    EXPECT_EQ(to_string(BlendMode::HardLight), "HardLight");
    EXPECT_EQ(to_string(BlendMode::SoftLight), "SoftLight");
    EXPECT_EQ(to_string(BlendMode::Difference), "Difference");
    EXPECT_EQ(to_string(BlendMode::Exclusion), "Exclusion");
    EXPECT_EQ(to_string(BlendMode::Add), "Add");
}

}  // namespace
}  // namespace nxtcut::model
