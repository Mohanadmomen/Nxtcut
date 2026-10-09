#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/track_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/track.hpp>

#include <gtest/gtest.h>

#include "test_helper.hpp"
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

TEST(TrackCommandsTest, AddTrackAppended) {
    core::UuidGenerator gen(301ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const std::size_t initial_count = editor.snapshot()->sequences.at(main_seq_id).tracks.size();

    AddTrack cmd{main_seq_id, model::TrackKind::Video, "NewVideoTrack", std::nullopt};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    ASSERT_EQ(receipt_res->created_tracks.size(), 1U);
    const auto created_track_id = receipt_res->created_tracks[0];

    const auto& tracks = editor.snapshot()->sequences.at(main_seq_id).tracks;
    ASSERT_EQ(tracks.size(), initial_count + 1);
    EXPECT_EQ(tracks.back().id, created_track_id);
    EXPECT_EQ(tracks.back().name, "NewVideoTrack");
    EXPECT_TRUE(tracks.back().clips.empty());
}

TEST(TrackCommandsTest, AddTrackAtIndexZeroBecomesFirst) {
    core::UuidGenerator gen(302ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    AddTrack cmd{main_seq_id, model::TrackKind::Audio, "FirstAudioTrack", 0U};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    ASSERT_EQ(receipt_res->created_tracks.size(), 1U);
    const auto created_track_id = receipt_res->created_tracks[0];

    const auto& tracks = editor.snapshot()->sequences.at(main_seq_id).tracks;
    EXPECT_EQ(tracks.front().id, created_track_id);
    EXPECT_EQ(tracks.front().name, "FirstAudioTrack");
}

TEST(TrackCommandsTest, AddTrackAtIndexEqualSizeSucceeds) {
    core::UuidGenerator gen(303ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const std::size_t count = editor.snapshot()->sequences.at(main_seq_id).tracks.size();

    AddTrack cmd{main_seq_id, model::TrackKind::Video, "EndTrack", count};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks.size(), count + 1);
}

TEST(TrackCommandsTest, AddTrackAtIndexSizePlusOneOutOfRange) {
    core::UuidGenerator gen(304ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const std::size_t count = editor.snapshot()->sequences.at(main_seq_id).tracks.size();

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    AddTrack cmd{main_seq_id, model::TrackKind::Video, "BadTrack", count + 1};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::OutOfRange);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(TrackCommandsTest, AddTrackUnknownSequenceNotFound) {
    core::UuidGenerator gen(305ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto unknown_seq = model::generate_id<model::SequenceId>(gen);
    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    AddTrack cmd{unknown_seq, model::TrackKind::Video, "BadTrack", std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(TrackCommandsTest, RemoveTrackClearsRemainingSingleLink) {
    core::UuidGenerator gen(306ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    // Find audio track (track 1 in fixture)
    model::TrackId audio_track_id{};
    for (const auto& trk : editor.snapshot()->sequences.at(main_seq_id).tracks) {
        if (trk.kind == model::TrackKind::Audio) {
            audio_track_id = trk.id;
            break;
        }
    }
    ASSERT_FALSE(audio_track_id.is_nil());

    RemoveTrack cmd{main_seq_id, audio_track_id};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    // After removing audio track, the video clip that was linked must now have nullopt link_id
    const auto& video_clip = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0];
    EXPECT_FALSE(video_clip.link_id.has_value());
}

TEST(TrackCommandsTest, RemoveTrackThreeWayLinkKeepsRemainingTwoLinked) {
    core::UuidGenerator gen(307ULL);
    model::Project p = model::test::build_valid_project(gen);

    // Add a second audio track to main sequence holding a clip with the same link_id
    auto& main_seq = p.sequences.at(p.main_sequence);
    const model::SequenceId main_seq_id = p.main_sequence;
    const auto existing_link = main_seq.tracks[0].clips[0].link_id;
    ASSERT_TRUE(existing_link.has_value());

    // Find the video media id
    const auto video_media_id =
        std::get<model::VideoContent>(main_seq.tracks[0].clips[0].content).media;

    model::TrackId extra_audio_track_id = model::generate_id<model::TrackId>(gen);
    model::Track extra_audio_track;
    extra_audio_track.id = extra_audio_track_id;
    extra_audio_track.name = "A2";
    extra_audio_track.kind = model::TrackKind::Audio;

    model::Clip extra_audio_clip;
    extra_audio_clip.id = model::generate_id<model::ClipId>(gen);
    extra_audio_clip.name = "ExtraAudio";
    extra_audio_clip.start = model::TimelineTime::zero();
    extra_audio_clip.duration = model::test::duration_of_seconds(5);
    extra_audio_clip.source_in = model::SourceTime::zero();
    extra_audio_clip.speed = model::Speed::normal();
    extra_audio_clip.link_id = existing_link;
    extra_audio_clip.content = model::AudioContent{video_media_id};

    extra_audio_track.clips.push_back(extra_audio_clip);
    main_seq.tracks.push_back(extra_audio_track);

    auto ed_res = Editor::create(std::move(p), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Now remove the video track (which holds the 1st linked clip).
    // The remaining 2 audio clips still share the link, so their link_ids must be PRESERVED.
    const auto video_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    RemoveTrack cmd{main_seq_id, video_track_id};

    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    // Check remaining tracks in main sequence
    const auto& remaining_tracks = editor.snapshot()->sequences.at(main_seq_id).tracks;
    ASSERT_GE(remaining_tracks.size(), 2U);
    EXPECT_EQ(remaining_tracks[0].clips[0].link_id, existing_link);
    EXPECT_EQ(remaining_tracks[1].clips[0].link_id, existing_link);
}

TEST(TrackCommandsTest, RemoveTrackLockedTrackFails) {
    core::UuidGenerator gen(308ULL);
    model::Project p = model::test::build_valid_project(gen);
    p.sequences.at(p.main_sequence).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(p), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto locked_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveTrack cmd{main_seq_id, locked_track_id};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string_view::npos);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(TrackCommandsTest, RemoveTrackUnknownTrackNotFound) {
    core::UuidGenerator gen(309ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto unknown_track_id = model::generate_id<model::TrackId>(gen);

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveTrack cmd{main_seq_id, unknown_track_id};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(TrackCommandsTest, MoveTrackToIndex0AndLastIndex) {
    core::UuidGenerator gen(310ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    // Add a 3rd track so we have 3 tracks: 0, 1, 2
    AddTrack add_cmd{main_seq_id, model::TrackKind::Video, "V2", std::nullopt};
    auto add_res = test::round_trip(editor, add_cmd);
    ASSERT_TRUE(test::is_ok(add_res));

    const auto t0_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto t1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].id;
    const auto t2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[2].id;

    // Move track 0 to last index (2)
    MoveTrack move_to_last{main_seq_id, t0_id, 2U};
    auto move_res1 = test::round_trip(editor, move_to_last);
    ASSERT_TRUE(test::is_ok(move_res1));

    {
        const auto& tracks = editor.snapshot()->sequences.at(main_seq_id).tracks;
        EXPECT_EQ(tracks[0].id, t1_id);
        EXPECT_EQ(tracks[1].id, t2_id);
        EXPECT_EQ(tracks[2].id, t0_id);
    }

    // Now move track t0 back to index 0
    MoveTrack move_to_first{main_seq_id, t0_id, 0U};
    auto move_res2 = test::round_trip(editor, move_to_first);
    ASSERT_TRUE(test::is_ok(move_res2));

    {
        const auto& tracks = editor.snapshot()->sequences.at(main_seq_id).tracks;
        EXPECT_EQ(tracks[0].id, t0_id);
        EXPECT_EQ(tracks[1].id, t1_id);
        EXPECT_EQ(tracks[2].id, t2_id);
    }
}

TEST(TrackCommandsTest, MoveTrackSameIndexGivesEmptyChangeSet) {
    core::UuidGenerator gen(311ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    MoveTrack cmd{main_seq_id, track_id, 0U};
    auto res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(res));
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(TrackCommandsTest, MoveTrackNewIndexEqualSizeOutOfRange) {
    core::UuidGenerator gen(312ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto& tracks = editor.snapshot()->sequences.at(main_seq_id).tracks;
    const auto track_id = tracks[0].id;
    const std::size_t size = tracks.size();

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    MoveTrack cmd{main_seq_id, track_id, size};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::OutOfRange);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(TrackCommandsTest, MoveTrackAllowedOnLockedTrack) {
    core::UuidGenerator gen(313ULL);
    model::Project p = model::test::build_valid_project(gen);
    p.sequences.at(p.main_sequence).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(p), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto locked_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    MoveTrack cmd{main_seq_id, locked_track_id, 1U};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[1].id, locked_track_id);
}

TEST(TrackCommandsTest, SetTrackPropertiesRenameDisableLockUnlock) {
    core::UuidGenerator gen(314ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    // 1. Rename
    SetTrackProperties rename_cmd{main_seq_id, track_id, "RenamedTrack", std::nullopt,
                                  std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, rename_cmd)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[0].name, "RenamedTrack");

    // 2. Disable
    SetTrackProperties disable_cmd{main_seq_id, track_id, std::nullopt, false, std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, disable_cmd)));
    EXPECT_FALSE(editor.snapshot()->sequences.at(main_seq_id).tracks[0].enabled);

    // 3. Lock
    SetTrackProperties lock_cmd{main_seq_id, track_id, std::nullopt, std::nullopt, true};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, lock_cmd)));
    EXPECT_TRUE(editor.snapshot()->sequences.at(main_seq_id).tracks[0].locked);

    // 4. Unlock locked track (allowed on locked tracks!)
    SetTrackProperties unlock_cmd{main_seq_id, track_id, std::nullopt, std::nullopt, false};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, unlock_cmd)));
    EXPECT_FALSE(editor.snapshot()->sequences.at(main_seq_id).tracks[0].locked);
}

TEST(TrackCommandsTest, SetTrackPropertiesAllEmptyInvalidArgument) {
    core::UuidGenerator gen(315ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetTrackProperties cmd{main_seq_id, track_id, std::nullopt, std::nullopt, std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(TrackCommandsTest, SetTrackPropertiesSameValuesNoHistoryEntry) {
    core::UuidGenerator gen(316ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto& track = editor.snapshot()->sequences.at(main_seq_id).tracks[0];

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetTrackProperties cmd{main_seq_id, track.id, track.name, track.enabled, track.locked};
    auto res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(res));
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

}  // namespace
}  // namespace nxtcut::commands
