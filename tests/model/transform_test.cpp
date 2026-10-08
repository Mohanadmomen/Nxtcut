#include <nxtcut/model/transform.hpp>

#include <gtest/gtest.h>

namespace nxtcut::model {
namespace {

TEST(TransformTest, DefaultValues) {
    const TransformProps props;
    const ClipTime t0 = ClipTime::zero();

    EXPECT_DOUBLE_EQ(props.position_x.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.position_y.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.scale_x.value_at(t0), 1.0);
    EXPECT_DOUBLE_EQ(props.scale_y.value_at(t0), 1.0);
    EXPECT_DOUBLE_EQ(props.rotation_degrees.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.anchor_x.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.anchor_y.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.opacity.value_at(t0), 1.0);
    EXPECT_DOUBLE_EQ(props.crop_left.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.crop_right.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.crop_top.value_at(t0), 0.0);
    EXPECT_DOUBLE_EQ(props.crop_bottom.value_at(t0), 0.0);
}

}  // namespace
}  // namespace nxtcut::model
