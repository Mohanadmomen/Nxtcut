#include <nxtcut/model/media.hpp>

#include <gtest/gtest.h>

namespace nxtcut::model {
namespace {

TEST(MediaTest, MediaKindToString) {
    EXPECT_EQ(to_string(MediaKind::Video), "Video");
    EXPECT_EQ(to_string(MediaKind::Audio), "Audio");
    EXPECT_EQ(to_string(MediaKind::Image), "Image");
}

TEST(MediaTest, MediaAssetStructure) {
    core::UuidGenerator gen(111ULL);
    const MediaId mid = generate_id<MediaId>(gen);

    MediaAsset asset;
    asset.id = mid;
    asset.name = "clip.mp4";
    asset.path = "/assets/clip.mp4";
    asset.kind = MediaKind::Video;
    asset.duration = core::Duration::from_seconds(30.0);
    asset.video = VideoStreamInfo{core::Size<std::int32_t>{3840, 2160}, core::frame_rates::k60};
    asset.audio = AudioStreamInfo{core::sample_rates::k48000, 6};

    EXPECT_EQ(asset.id, mid);
    EXPECT_EQ(asset.name, "clip.mp4");
    EXPECT_EQ(asset.path, "/assets/clip.mp4");
    EXPECT_EQ(asset.kind, MediaKind::Video);
    EXPECT_EQ(asset.duration.ticks(), core::Duration::from_seconds(30.0).ticks());
    ASSERT_TRUE(asset.video.has_value());
    EXPECT_EQ(asset.video->size.width, 3840);
    EXPECT_EQ(asset.video->size.height, 2160);
    EXPECT_EQ(asset.video->frame_rate, core::frame_rates::k60);
    ASSERT_TRUE(asset.audio.has_value());
    EXPECT_EQ(asset.audio->sample_rate, core::sample_rates::k48000);
    EXPECT_EQ(asset.audio->channels, 6);
}

}  // namespace
}  // namespace nxtcut::model
