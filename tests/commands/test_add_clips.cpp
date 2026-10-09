#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include "test_helper.hpp"
#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(AddClipsTest, AddImageClipAt15s) {
    core::UuidGenerator gen(1001ULL);
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

    AddClips cmd{main_seq_id, timeline_at_seconds(15), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));
    ASSERT_EQ(rt_res.value().created_clips.size(), 1U);

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 4U);
    EXPECT_EQ(clips[3].start, timeline_at_seconds(15));
    EXPECT_EQ(clips[3].duration, duration_of_seconds(3));
    EXPECT_FALSE(clips[3].link_id.has_value());
}

TEST(AddClipsTest, OverlapAt4sFailsWithOverlapMessage) {
    core::UuidGenerator gen(1002ULL);
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

    AddClips cmd{main_seq_id, timeline_at_seconds(4), {{v_track_id, new_img}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("overlap"), std::string::npos);
}

TEST(AddClipsTest, VideoPlusAudioGroupGetsOneFreshLinkId) {
    core::UuidGenerator gen(1003ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto a_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].id;
    const auto video_media_id =
        std::get<model::VideoContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    const auto existing_link =
        editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].link_id;
    ASSERT_TRUE(existing_link.has_value());

    model::Clip v_clip;
    v_clip.duration = duration_of_seconds(4);
    v_clip.content = model::VideoContent{video_media_id};

    model::Clip a_clip;
    a_clip.duration = duration_of_seconds(4);
    a_clip.content = model::AudioContent{video_media_id};

    AddClips cmd{
        main_seq_id, timeline_at_seconds(20), {{v_track_id, v_clip}, {a_track_id, a_clip}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    ASSERT_EQ(v_clips.size(), 4U);
    ASSERT_EQ(a_clips.size(), 2U);

    ASSERT_TRUE(v_clips[3].link_id.has_value());
    ASSERT_TRUE(a_clips[1].link_id.has_value());
    EXPECT_EQ(v_clips[3].link_id, a_clips[1].link_id);
    EXPECT_NE(v_clips[3].link_id, existing_link);
}

TEST(AddClipsTest, LockedDestinationFailsWithLockedMessage) {
    core::UuidGenerator gen(1004ULL);
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
    new_img.duration = duration_of_seconds(3);
    new_img.content = model::ImageContent{image_media_id};

    AddClips cmd{main_seq_id, timeline_at_seconds(15), {{v_track_id, new_img}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(AddClipsTest, AudioClipOntoVideoTrackGivesInvalidArgument) {
    core::UuidGenerator gen(1005ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto video_media_id =
        std::get<model::VideoContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    model::Clip a_clip;
    a_clip.duration = duration_of_seconds(3);
    a_clip.content = model::AudioContent{video_media_id};

    AddClips cmd{main_seq_id, timeline_at_seconds(15), {{v_track_id, a_clip}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(AddClipsTest, CompoundClipCycleGivesInvalidArgument) {
    core::UuidGenerator gen(1006ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    model::Clip comp_clip;
    comp_clip.duration = duration_of_seconds(3);
    comp_clip.content = model::CompoundContent{main_seq_id};

    AddClips cmd{main_seq_id, timeline_at_seconds(15), {{v_track_id, comp_clip}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(AddClipsTest, SourceSpanExceedsMediaDurationGivesInvalidArgument) {
    core::UuidGenerator gen(1007ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto video_media_id =
        std::get<model::VideoContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    model::Clip v_clip;
    v_clip.duration = duration_of_seconds(70);  // media duration is 60s
    v_clip.content = model::VideoContent{video_media_id};

    AddClips cmd{main_seq_id, timeline_at_seconds(15), {{v_track_id, v_clip}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(AddClipsTest, EmptyListGivesInvalidArgument) {
    core::UuidGenerator gen(1008ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    AddClips cmd{main_seq_id, timeline_at_seconds(15), {}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(AddClipsTest, UnknownTrackGivesNotFound) {
    core::UuidGenerator gen(1009ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto fake_track_id = model::generate_id<model::TrackId>(gen);
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip new_img;
    new_img.duration = duration_of_seconds(3);
    new_img.content = model::ImageContent{image_media_id};

    AddClips cmd{main_seq_id, timeline_at_seconds(15), {{fake_track_id, new_img}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
}

TEST(AddClipsTest, AtWithOneTickOfNoiseSnapsToFrameBoundary) {
    core::UuidGenerator gen(1010ULL);
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

    const auto noisy_at = model::TimelineTime::from_ticks(timeline_at_seconds(15).ticks() + 1);

    AddClips cmd{main_seq_id, noisy_at, {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 4U);
    EXPECT_EQ(clips[3].start, timeline_at_seconds(15));
}

}  // namespace
}  // namespace nxtcut::commands
