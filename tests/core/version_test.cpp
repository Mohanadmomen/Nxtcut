#include <nxtcut/core/version.hpp>

#include <gtest/gtest.h>

namespace nxtcut::core {
namespace {

TEST(VersionTest, VersionReturnsValidSemanticVersion) {
    const Version ver = version();
    EXPECT_EQ(ver.major, 0U);
    EXPECT_EQ(ver.minor, 1U);
    EXPECT_EQ(ver.patch, 0U);
}

TEST(VersionTest, ToStringMemberMethodFormatsCorrectly) {
    const Version ver{
        .major = 1,
        .minor = 2,
        .patch = 3,
    };
    EXPECT_EQ(ver.to_string(), "1.2.3");
}

TEST(VersionTest, ToStringFreeFunctionFormatsCorrectly) {
    const Version ver{
        .major = 2,
        .minor = 0,
        .patch = 4,
    };
    EXPECT_EQ(to_string(ver), "2.0.4");
}

TEST(VersionTest, VersionEqualityAndOrderingWork) {
    constexpr Version v1{.major = 1, .minor = 0, .patch = 0};
    constexpr Version v2{.major = 1, .minor = 0, .patch = 0};
    constexpr Version v3{.major = 1, .minor = 1, .patch = 0};
    constexpr Version v4{.major = 2, .minor = 0, .patch = 0};

    EXPECT_EQ(v1, v2);
    EXPECT_NE(v1, v3);
    EXPECT_LT(v1, v3);
    EXPECT_LT(v3, v4);
    EXPECT_GT(v4, v1);
    EXPECT_LE(v1, v2);
    EXPECT_GE(v4, v3);
}

}  // namespace
}  // namespace nxtcut::core
