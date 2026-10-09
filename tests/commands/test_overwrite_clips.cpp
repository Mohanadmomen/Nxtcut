#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "test_helper.hpp"
#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(OverwriteClipsTest, OverwriteMiddleOfV2At6to8sSplitsV2) {
    core::UuidGenerator gen(3001ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(2);
    new_img.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(6), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 5U);
    // V1: 0..5s
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    // V2 left: 5..6s
    EXPECT_EQ(clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(clips[1].duration, duration_of_seconds(1));
    // New clip: 6..8s
    EXPECT_EQ(clips[2].start, timeline_at_seconds(6));
    EXPECT_EQ(clips[2].duration, duration_of_seconds(2));
    // V2 right: 8..10s
    EXPECT_EQ(clips[3].start, timeline_at_seconds(8));
    EXPECT_EQ(clips[3].duration, duration_of_seconds(2));
    // V3: 10..15s
    EXPECT_EQ(clips[4].start, timeline_at_seconds(10));
}

TEST(OverwriteClipsTest, OverwriteAcrossV1andV2At4to6sTrimsBoth) {
    core::UuidGenerator gen(3002ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto video_media_id =
        std::get<model::VideoContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    model::Clip new_vid;
    new_vid.duration = duration_of_seconds(2);
    new_vid.content = model::VideoContent{video_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(4), {{v_track_id, new_vid}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 4U);
    // V1 tail trimmed: 0..4s
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(clips[0].duration, duration_of_seconds(4));
    // New clip: 4..6s
    EXPECT_EQ(clips[1].start, timeline_at_seconds(4));
    EXPECT_EQ(clips[1].duration, duration_of_seconds(2));
    // V2 head trimmed: 6..10s
    EXPECT_EQ(clips[2].start, timeline_at_seconds(6));
    EXPECT_EQ(clips[2].duration, duration_of_seconds(4));
    // V3: 10..15s
    EXPECT_EQ(clips[3].start, timeline_at_seconds(10));
}

TEST(OverwriteClipsTest, OverwriteExactlyOver5to10sRemovesV2) {
    core::UuidGenerator gen(3003ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(5);
    new_img.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(5), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 3U);
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(clips[1].duration, duration_of_seconds(5));
    EXPECT_EQ(clips[2].start, timeline_at_seconds(10));
}

TEST(OverwriteClipsTest, OverwriteAllOfV1ClearsA1LinkId) {
    core::UuidGenerator gen(3004ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(5);
    new_img.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(0), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    ASSERT_EQ(a_clips.size(), 1U);
    EXPECT_FALSE(a_clips[0].link_id.has_value());
}

TEST(OverwriteClipsTest, OverwriteEmptyRegionEqualsAddClips) {
    core::UuidGenerator gen(3005ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(3);
    new_img.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(15), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 4U);
    EXPECT_EQ(clips[3].start, timeline_at_seconds(15));
    EXPECT_EQ(clips[3].duration, duration_of_seconds(3));
}

TEST(OverwriteClipsTest, Speed2ClipTrimmedWithCorrectSourceIn) {
    core::UuidGenerator gen(3006ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Set V1 to speed 2/1, duration 5s (consumes 10s of source)
    project.sequences.at(main_seq_id).tracks[0].clips[0].speed = model::Speed::create(2, 1).value();

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    // Overwrite head of V1: 0..1s with an image clip
    // V1 was 0..5s. Now V1 is 1..5s (head trimmed by 1s)
    // source_in should advance by 1s * speed(2) = 2s
    model::Clip new_img;
    new_img.duration = duration_of_seconds(1);
    new_img.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(0), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    // clips[0] is new_img (0..1s), clips[1] is trimmed V1 (1..5s)
    EXPECT_EQ(clips[1].start, timeline_at_seconds(1));
    EXPECT_EQ(clips[1].duration, duration_of_seconds(4));
    EXPECT_EQ(clips[1].source_in.ticks(), timeline_at_seconds(2).ticks());
}

TEST(OverwriteClipsTest, LockedDestinationFailsWithLockedMessage) {
    core::UuidGenerator gen(3007ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(2);
    new_img.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(6), {{v_track_id, new_img}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(OverwriteClipsTest, OverwriteClipsExtremeTimeFailsWithoutThrow) {
    core::UuidGenerator gen(3008ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(5);
    new_img.content = model::ImageContent{image_media_id};

    const auto extreme_at = model::TimelineTime::from_ticks(
        std::numeric_limits<std::int64_t>::max() - core::kTicksPerSecond / 2);
    OverwriteClips cmd{main_seq_id, extreme_at, {{v_track_id, new_img}}};

    core::Result<EditReceipt> res;
    EXPECT_NO_THROW({ res = editor.execute(cmd); });
    EXPECT_FALSE(res.has_value());
}

TEST(OverwriteClipsTest, OverwriteAudioClipStrictlyContainsSplitsWithFades) {
    core::UuidGenerator gen(3009ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    auto& a1 = project.sequences.at(main_seq_id).tracks[1].clips[0];
    auto& a1_content = std::get<model::AudioContent>(a1.content);
    a1_content.fade_in = duration_of_seconds(1);
    a1_content.fade_out = duration_of_seconds(1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto a_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].id;
    const auto video_media_id =
        std::get<model::AudioContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].content)
            .media;

    model::Clip new_audio;
    new_audio.duration = duration_of_seconds(1);
    new_audio.content = model::AudioContent{video_media_id};

    // Strictly contains-splits A1 (0..5s) with a 1s audio clip placed at 2s (2..3s)
    OverwriteClips cmd{main_seq_id, timeline_at_seconds(2), {{a_track_id, new_audio}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    ASSERT_EQ(a_clips.size(), 3U);
    // Left: 0..2s
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(2));
    // Placed: 2..3s
    EXPECT_EQ(a_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(a_clips[1].duration, duration_of_seconds(1));
    // Right: 3..5s
    EXPECT_EQ(a_clips[2].start, timeline_at_seconds(3));
    EXPECT_EQ(a_clips[2].duration, duration_of_seconds(2));
}

TEST(OverwriteClipsTest, DuplicateDestinationTrackFailsWithDuplicateMessage) {
    core::UuidGenerator gen(3010ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip clip1;
    clip1.duration = duration_of_seconds(1);
    clip1.content = model::ImageContent{image_media_id};

    model::Clip clip2;
    clip2.duration = duration_of_seconds(2);
    clip2.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{
        main_seq_id, timeline_at_seconds(0), {{v_track_id, clip1}, {v_track_id, clip2}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("duplicate"), std::string::npos);
}

TEST(OverwriteClipsTest, OverwriteHeadOfV1MaintainsLinkWithA1) {
    core::UuidGenerator gen(3011ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(2);
    new_img.content = model::ImageContent{image_media_id};

    OverwriteClips cmd{main_seq_id, timeline_at_seconds(0), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    // v_clips[0] is new_img (0..2s)
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(2));

    // v_clips[1] is trimmed V1: start 2s, duration 3s, source_in = 2s
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(3));
    EXPECT_EQ(v_clips[1].source_in.ticks(), timeline_at_seconds(2).ticks());

    // A1 keeps its link and V1 and A1 still form a group
    ASSERT_TRUE(v_clips[1].link_id.has_value());
    ASSERT_TRUE(a_clips[0].link_id.has_value());
    EXPECT_EQ(v_clips[1].link_id, a_clips[0].link_id);
}

}  // namespace
}  // namespace nxtcut::commands
