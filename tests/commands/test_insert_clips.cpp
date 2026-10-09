#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>
#include "test_helper.hpp"

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(InsertClipsTest, InsertImageAt7sSplitsV2AndShifts) {
    core::UuidGenerator gen(2001ULL);
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

    InsertClips cmd{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 5U);
    // V1: 0..5s
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(clips[0].duration, duration_of_seconds(5));
    // V2 left: 5..7s
    EXPECT_EQ(clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(clips[1].duration, duration_of_seconds(2));
    // New inserted clip: 7..9s
    EXPECT_EQ(clips[2].start, timeline_at_seconds(7));
    EXPECT_EQ(clips[2].duration, duration_of_seconds(2));
    // V2 right: 9..12s (new id)
    EXPECT_EQ(clips[3].start, timeline_at_seconds(9));
    EXPECT_EQ(clips[3].duration, duration_of_seconds(3));
    // V3: 12..17s
    EXPECT_EQ(clips[4].start, timeline_at_seconds(12));
    EXPECT_EQ(clips[4].duration, duration_of_seconds(5));
}

TEST(InsertClipsTest, InsertAtBoundary5sSplitsNothing) {
    core::UuidGenerator gen(2002ULL);
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

    InsertClips cmd{main_seq_id, timeline_at_seconds(5), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 4U);
    // V1: 0..5s
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(clips[0].duration, duration_of_seconds(5));
    // New clip: 5..7s
    EXPECT_EQ(clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(clips[1].duration, duration_of_seconds(2));
    // V2: 7..12s
    EXPECT_EQ(clips[2].start, timeline_at_seconds(7));
    EXPECT_EQ(clips[2].duration, duration_of_seconds(5));
    // V3: 12..17s
    EXPECT_EQ(clips[3].start, timeline_at_seconds(12));
    EXPECT_EQ(clips[3].duration, duration_of_seconds(5));
}

TEST(InsertClipsTest, InsertAtEnd15sShiftsNothing) {
    core::UuidGenerator gen(2003ULL);
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

    InsertClips cmd{main_seq_id, timeline_at_seconds(15), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 4U);
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(clips[2].start, timeline_at_seconds(10));
    EXPECT_EQ(clips[3].start, timeline_at_seconds(15));
}

TEST(InsertClipsTest, InsertAt2sVideoAudioGroupSplitsLinkedV1AndA1) {
    core::UuidGenerator gen(2004ULL);
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
    const auto old_link_id =
        editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].link_id;
    ASSERT_TRUE(old_link_id.has_value());

    model::Clip v_clip;
    v_clip.duration = duration_of_seconds(3);
    v_clip.content = model::VideoContent{video_media_id};

    model::Clip a_clip;
    a_clip.duration = duration_of_seconds(3);
    a_clip.content = model::AudioContent{video_media_id};

    InsertClips cmd{main_seq_id, timeline_at_seconds(2), {{v_track_id, v_clip}, {a_track_id, a_clip}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    // V1 left: 0..2s, keeps old link
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(2));
    EXPECT_EQ(v_clips[0].link_id, old_link_id);

    // Inserted video: 2..5s
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(3));
    ASSERT_TRUE(v_clips[1].link_id.has_value());

    // V1 right: (2 + 3).. (5 + 3) = 5..8s, fresh link
    EXPECT_EQ(v_clips[2].start, timeline_at_seconds(5));
    EXPECT_EQ(v_clips[2].duration, duration_of_seconds(3));
    ASSERT_TRUE(v_clips[2].link_id.has_value());

    // A1 left: 0..2s, keeps old link
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(2));
    EXPECT_EQ(a_clips[0].link_id, old_link_id);

    // Inserted audio: 2..5s
    EXPECT_EQ(a_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(a_clips[1].duration, duration_of_seconds(3));
    EXPECT_EQ(a_clips[1].link_id, v_clips[1].link_id);

    // A1 right: 5..8s
    EXPECT_EQ(a_clips[2].start, timeline_at_seconds(5));
    EXPECT_EQ(a_clips[2].duration, duration_of_seconds(3));
    EXPECT_EQ(a_clips[2].link_id, v_clips[2].link_id);

    EXPECT_NE(v_clips[1].link_id, old_link_id);
    EXPECT_NE(v_clips[2].link_id, old_link_id);
    EXPECT_NE(v_clips[1].link_id, v_clips[2].link_id);
}

TEST(InsertClipsTest, ScopeEditedTracksOnlyVsAllUnlockedTracks) {
    core::UuidGenerator gen(2005ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Add a 3rd track (audio track 2) with a clip at 10..15s
    const auto extra_track_id = model::generate_id<model::TrackId>(gen);
    model::Track extra_track;
    extra_track.id = extra_track_id;
    extra_track.name = "A2";
    extra_track.kind = model::TrackKind::Audio;
    extra_track.enabled = true;
    extra_track.locked = false;

    const auto video_media_id =
        std::get<model::VideoContent>(
            project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    model::Clip extra_clip;
    extra_clip.id = model::generate_id<model::ClipId>(gen);
    extra_clip.start = timeline_at_seconds(10);
    extra_clip.duration = duration_of_seconds(5);
    extra_clip.content = model::AudioContent{video_media_id};
    extra_track.clips.push_back(extra_clip);
    project.sequences.at(main_seq_id).tracks.push_back(extra_track);

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

    // 1. With EditedTracksOnly: clip on 3rd track does NOT move
    InsertClips cmd_edited{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}},
                           RippleScope::EditedTracksOnly};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_edited)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[2].clips[0].start,
              timeline_at_seconds(10));

    // Undo to return to baseline
    static_cast<void>(editor.undo());

    // 2. With AllUnlockedTracks: clip on 3rd track DOES move
    InsertClips cmd_all{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}},
                        RippleScope::AllUnlockedTracks};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_all)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[2].clips[0].start,
              timeline_at_seconds(12));
}

TEST(InsertClipsTest, LockedNonDestinationTrackUntouchedSucceeds) {
    core::UuidGenerator gen(2006ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    // Lock track 1 (Audio)
    project.sequences.at(main_seq_id).tracks[1].locked = true;

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

    // Insert at 7s on video track (V2 is unlinked image clip)
    InsertClips cmd{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    // Audio track clips are untouched
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    ASSERT_EQ(a_clips.size(), 1U);
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
}

TEST(InsertClipsTest, LockedDestinationTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(2007ULL);
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

    InsertClips cmd{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(InsertClipsTest, TwoClipsOf2sAnd3sShiftLaterClipsBy3s) {
    core::UuidGenerator gen(2008ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Add a second video track V2
    const auto v2_track_id = model::generate_id<model::TrackId>(gen);
    model::Track v2_track;
    v2_track.id = v2_track_id;
    v2_track.name = "V2_Track";
    v2_track.kind = model::TrackKind::Video;
    v2_track.enabled = true;
    v2_track.locked = false;
    project.sequences.at(main_seq_id).tracks.push_back(v2_track);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip clip2s;
    clip2s.duration = duration_of_seconds(2);
    clip2s.content = model::ImageContent{image_media_id};

    model::Clip clip3s;
    clip3s.duration = duration_of_seconds(3);
    clip3s.content = model::ImageContent{image_media_id};

    // Insert at 5s: V2 on main track was at 5..10s, V3 at 10..15s.
    // delta = max(2s, 3s) = 3s.
    // V2 shifts to 8..13s, V3 shifts to 13..18s.
    InsertClips cmd{main_seq_id, timeline_at_seconds(5),
                    {{v1_track_id, clip2s}, {v2_track_id, clip3s}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    // V1 (0..5s), new clip (5..7s), V2 (8..13s), V3 (13..18s)
    ASSERT_EQ(clips.size(), 4U);
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(clips[1].duration, duration_of_seconds(2));
    EXPECT_EQ(clips[2].start, timeline_at_seconds(8));
    EXPECT_EQ(clips[3].start, timeline_at_seconds(13));
}

}  // namespace
}  // namespace nxtcut::commands
