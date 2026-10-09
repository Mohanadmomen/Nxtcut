#include <nxtcut/core/color.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/effect.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/property.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>
#include <nxtcut/model/track.hpp>
#include <nxtcut/model/transform.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <variant>

#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::model {
namespace {

TEST(EqualityTest, CopyOfFixtureIsIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    const Project copy = orig;
    EXPECT_TRUE(identical(orig, copy));
}

TEST(EqualityTest, MutationProjectNameNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.name = "DifferentProjectName";
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationSequenceNameNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].name = "DifferentSequenceName";
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationSequenceCanvasNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].canvas.width += 1;
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationTrackLockedNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].tracks[0].locked = true;
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationClipStartNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].tracks[0].clips[0].start =
        TimelineTime::from_ticks(100);
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationClipSpeedNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].tracks[0].clips[0].speed = Speed::create(2, 1).value();
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationClipNameNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].tracks[0].clips[0].name = "MutatedClipName";
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationTransformOpacityNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].tracks[0].clips[0].transform.opacity.set_constant(0.5);
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationEffectParamNotIdentical) {
    Project orig = test::build_valid_project(42ULL);
    core::UuidGenerator gen(9999ULL);
    EffectInstance effect;
    effect.id = generate_id<EffectId>(gen);
    effect.effect_type = "builtin.gaussian_blur";
    effect.enabled = true;
    effect.params["radius"] = Property<double>(5.0);

    orig.sequences[orig.main_sequence].tracks[0].clips[0].effects.push_back(effect);

    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].tracks[0].clips[0].effects[0].params["radius"] =
        Property<double>(10.0);

    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationContentAlternativeNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    auto& clip = mutated.sequences[mutated.main_sequence].tracks[0].clips[0];
    const MediaId vid = std::get<VideoContent>(clip.content).media;
    clip.content = AudioContent{vid};
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationTextContentStringNotIdentical) {
    Project orig = test::build_valid_project(42ULL);
    core::UuidGenerator gen(8888ULL);
    Clip txt_clip;
    txt_clip.id = generate_id<ClipId>(gen);
    txt_clip.name = "TextOverlay";
    txt_clip.start = TimelineTime::zero();
    txt_clip.duration = core::Duration::from_seconds(2.0);
    txt_clip.source_in = SourceTime::zero();
    txt_clip.speed = Speed::normal();
    txt_clip.content = TextContent{"OriginalText"};

    orig.sequences[orig.main_sequence].tracks[0].clips.push_back(txt_clip);

    Project mutated = orig;
    std::get<TextContent>(mutated.sequences[mutated.main_sequence].tracks[0].clips.back().content)
        .text = "ChangedText";
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationMediaPathNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    for (auto& [mid, asset] : mutated.media) {
        if (asset.kind == MediaKind::Video) {
            asset.path = "/changed/path.mp4";
            break;
        }
    }
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationMarkerLabelNotIdentical) {
    Project orig = test::build_valid_project(42ULL);
    core::UuidGenerator gen(7777ULL);
    Marker marker;
    marker.id = generate_id<MarkerId>(gen);
    marker.time = TimelineTime::zero();
    marker.label = "InitialLabel";
    orig.sequences[orig.main_sequence].markers.push_back(marker);

    Project mutated = orig;
    mutated.sequences[mutated.main_sequence].markers[0].label = "ChangedLabel";
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, MutationExtraMediaEntryNotIdentical) {
    const Project orig = test::build_valid_project(42ULL);
    Project mutated = orig;
    core::UuidGenerator gen(6666ULL);
    MediaAsset extra;
    extra.id = generate_id<MediaId>(gen);
    extra.name = "extra.png";
    extra.path = "/media/extra.png";
    extra.kind = MediaKind::Image;
    mutated.media[extra.id] = extra;
    EXPECT_FALSE(identical(orig, mutated));
}

TEST(EqualityTest, PropertyZeroVsNegativeZeroNotIdentical) {
    const Property<double> p_pos(0.0);
    const Property<double> p_neg(-0.0);
    EXPECT_FALSE(identical(p_pos, p_neg));
}

TEST(EqualityTest, PropertyIdenticalNaNAreIdentical) {
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    const Property<double> p_nan1(nan_val);
    const Property<double> p_nan2(nan_val);
    EXPECT_TRUE(identical(p_nan1, p_nan2));
}

}  // namespace
}  // namespace nxtcut::model
