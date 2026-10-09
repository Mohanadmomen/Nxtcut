#include <nxtcut/commands/apply.hpp>
#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/track.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

TEST(ApplyTest, ClipOrderingAddBetweenLandsBetween) {
    core::UuidGenerator gen(201ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId track_id = model::generate_id<model::TrackId>(gen);
    const model::ClipId clip0_id = model::generate_id<model::ClipId>(gen);
    const model::ClipId clip10_id = model::generate_id<model::ClipId>(gen);
    const model::ClipId clip6_id = model::generate_id<model::ClipId>(gen);

    model::Project p;
    p.main_sequence = seq_id;
    model::Sequence seq;
    seq.id = seq_id;
    model::Track trk;
    trk.id = track_id;

    model::Clip c0;
    c0.id = clip0_id;
    c0.start = model::TimelineTime::zero();
    c0.duration = model::test::duration_of_seconds(4);

    model::Clip c10;
    c10.id = clip10_id;
    c10.start = model::test::timeline_at_seconds(10);
    c10.duration = model::test::duration_of_seconds(4);

    trk.clips.push_back(c0);
    trk.clips.push_back(c10);
    seq.tracks.push_back(trk);
    p.sequences[seq_id] = seq;

    const model::Project orig = p;

    // Add clip at 6 s
    model::Clip c6;
    c6.id = clip6_id;
    c6.start = model::test::timeline_at_seconds(6);
    c6.duration = model::test::duration_of_seconds(2);

    ChangeSet cs;
    cs.label = "AddClipAt6";
    cs.changes.push_back(ClipChange{seq_id, track_id, clip6_id, std::nullopt, c6});

    auto status = apply(p, cs);
    ASSERT_TRUE(test::is_ok(status));

    const auto& clips_after = p.sequences[seq_id].tracks[0].clips;
    ASSERT_EQ(clips_after.size(), 3U);
    EXPECT_EQ(clips_after[0].id, clip0_id);
    EXPECT_EQ(clips_after[1].id, clip6_id);
    EXPECT_EQ(clips_after[2].id, clip10_id);

    // Inverse restores original
    status = apply(p, inverse(cs));
    ASSERT_TRUE(test::is_ok(status));
    EXPECT_TRUE(model::identical(p, orig));
}

TEST(ApplyTest, ClipOrderingAddAt20LandsLast) {
    core::UuidGenerator gen(202ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId track_id = model::generate_id<model::TrackId>(gen);
    const model::ClipId clip0_id = model::generate_id<model::ClipId>(gen);
    const model::ClipId clip10_id = model::generate_id<model::ClipId>(gen);
    const model::ClipId clip20_id = model::generate_id<model::ClipId>(gen);

    model::Project p;
    p.main_sequence = seq_id;
    model::Sequence seq;
    seq.id = seq_id;
    model::Track trk;
    trk.id = track_id;

    model::Clip c0;
    c0.id = clip0_id;
    c0.start = model::TimelineTime::zero();
    c0.duration = model::test::duration_of_seconds(4);

    model::Clip c10;
    c10.id = clip10_id;
    c10.start = model::test::timeline_at_seconds(10);
    c10.duration = model::test::duration_of_seconds(4);

    trk.clips.push_back(c0);
    trk.clips.push_back(c10);
    seq.tracks.push_back(trk);
    p.sequences[seq_id] = seq;

    const model::Project orig = p;

    // Add clip at 20 s
    model::Clip c20;
    c20.id = clip20_id;
    c20.start = model::test::timeline_at_seconds(20);
    c20.duration = model::test::duration_of_seconds(2);

    ChangeSet cs;
    cs.label = "AddClipAt20";
    cs.changes.push_back(ClipChange{seq_id, track_id, clip20_id, std::nullopt, c20});

    auto status = apply(p, cs);
    ASSERT_TRUE(test::is_ok(status));

    const auto& clips_after = p.sequences[seq_id].tracks[0].clips;
    ASSERT_EQ(clips_after.size(), 3U);
    EXPECT_EQ(clips_after[0].id, clip0_id);
    EXPECT_EQ(clips_after[1].id, clip10_id);
    EXPECT_EQ(clips_after[2].id, clip20_id);

    status = apply(p, inverse(cs));
    ASSERT_TRUE(test::is_ok(status));
    EXPECT_TRUE(model::identical(p, orig));
}

TEST(ApplyTest, ClipOrderingModifyStartsAt16MovesToEnd) {
    core::UuidGenerator gen(203ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId track_id = model::generate_id<model::TrackId>(gen);
    const model::ClipId clip0_id = model::generate_id<model::ClipId>(gen);
    const model::ClipId clip10_id = model::generate_id<model::ClipId>(gen);

    model::Project p;
    p.main_sequence = seq_id;
    model::Sequence seq;
    seq.id = seq_id;
    model::Track trk;
    trk.id = track_id;

    model::Clip c0;
    c0.id = clip0_id;
    c0.start = model::TimelineTime::zero();
    c0.duration = model::test::duration_of_seconds(4);

    model::Clip c10;
    c10.id = clip10_id;
    c10.start = model::test::timeline_at_seconds(10);
    c10.duration = model::test::duration_of_seconds(4);

    trk.clips.push_back(c0);
    trk.clips.push_back(c10);
    seq.tracks.push_back(trk);
    p.sequences[seq_id] = seq;

    const model::Project orig = p;

    // Modify c0 so it starts at 16 s
    model::Clip c0_mod = c0;
    c0_mod.start = model::test::timeline_at_seconds(16);

    ChangeSet cs;
    cs.label = "MoveC0To16";
    cs.changes.push_back(ClipChange{seq_id, track_id, clip0_id, c0, c0_mod});

    auto status = apply(p, cs);
    ASSERT_TRUE(test::is_ok(status));

    const auto& clips_after = p.sequences[seq_id].tracks[0].clips;
    ASSERT_EQ(clips_after.size(), 2U);
    EXPECT_EQ(clips_after[0].id, clip10_id);
    EXPECT_EQ(clips_after[1].id, clip0_id);
    EXPECT_EQ(clips_after[1].start.ticks(), 16 * core::kTicksPerSecond);

    status = apply(p, inverse(cs));
    ASSERT_TRUE(test::is_ok(status));
    EXPECT_TRUE(model::identical(p, orig));
}

TEST(ApplyTest, DuplicateAddGivesAlreadyExists) {
    core::UuidGenerator gen(204ULL);
    const model::Project orig = model::test::build_valid_project(gen);
    model::Project p = orig;

    const auto& clip = p.sequences.at(p.main_sequence).tracks[0].clips[0];

    ChangeSet cs;
    cs.label = "DuplicateAdd";
    cs.changes.push_back(ClipChange{p.main_sequence, p.sequences.at(p.main_sequence).tracks[0].id,
                                    clip.id, std::nullopt, clip});

    auto status = apply(p, cs);
    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code(), core::ErrorCode::AlreadyExists);
}

TEST(ApplyTest, RemoveUnknownIdGivesNotFound) {
    core::UuidGenerator gen(205ULL);
    const model::Project orig = model::test::build_valid_project(gen);
    model::Project p = orig;

    const model::ClipId unknown_id = model::generate_id<model::ClipId>(gen);
    model::Clip dummy;
    dummy.id = unknown_id;

    ChangeSet cs;
    cs.label = "RemoveUnknown";
    cs.changes.push_back(ClipChange{p.main_sequence, p.sequences.at(p.main_sequence).tracks[0].id,
                                    unknown_id, dummy, std::nullopt});

    auto status = apply(p, cs);
    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code(), core::ErrorCode::NotFound);
}

TEST(ApplyTest, TrackMoveAndInverseRestoreTrackOrder) {
    core::UuidGenerator gen(206ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId t0_id = model::generate_id<model::TrackId>(gen);
    const model::TrackId t1_id = model::generate_id<model::TrackId>(gen);
    const model::TrackId t2_id = model::generate_id<model::TrackId>(gen);

    model::Project p;
    p.main_sequence = seq_id;
    model::Sequence seq;
    seq.id = seq_id;

    model::Track t0;
    t0.id = t0_id;
    t0.name = "T0";
    model::Track t1;
    t1.id = t1_id;
    t1.name = "T1";
    model::Track t2;
    t2.id = t2_id;
    t2.name = "T2";

    seq.tracks.push_back(t0);
    seq.tracks.push_back(t1);
    seq.tracks.push_back(t2);
    p.sequences[seq_id] = seq;

    const model::Project orig = p;

    // Move track 0 to index 2
    ChangeSet cs;
    cs.label = "MoveT0";
    cs.changes.push_back(TrackMove{seq_id, t0_id, 0, 2});

    auto status = apply(p, cs);
    ASSERT_TRUE(test::is_ok(status));

    const auto& tracks_after = p.sequences[seq_id].tracks;
    ASSERT_EQ(tracks_after.size(), 3U);
    EXPECT_EQ(tracks_after[0].id, t1_id);
    EXPECT_EQ(tracks_after[1].id, t2_id);
    EXPECT_EQ(tracks_after[2].id, t0_id);

    status = apply(p, inverse(cs));
    ASSERT_TRUE(test::is_ok(status));
    EXPECT_TRUE(model::identical(p, orig));
}

TEST(ApplyTest, TwoModificationsOfSameClipInvertedRestoresOriginal) {
    core::UuidGenerator gen(207ULL);
    const model::Project orig = model::test::build_valid_project(gen);
    model::Project p = orig;

    const auto track_id = p.sequences.at(p.main_sequence).tracks[0].id;
    const auto clip0 = p.sequences.at(p.main_sequence).tracks[0].clips[0];

    model::Clip mod1 = clip0;
    mod1.name = "IntermediateName";
    mod1.duration = model::test::duration_of_seconds(7);

    model::Clip mod2 = mod1;
    mod2.name = "FinalName";
    mod2.duration = model::test::duration_of_seconds(9);

    ChangeSet cs;
    cs.label = "TwoModifications";
    cs.changes.push_back(ClipChange{p.main_sequence, track_id, clip0.id, clip0, mod1});
    cs.changes.push_back(ClipChange{p.main_sequence, track_id, clip0.id, mod1, mod2});

    auto status = apply(p, cs);
    ASSERT_TRUE(test::is_ok(status));

    const auto& clip_after = p.sequences.at(p.main_sequence).tracks[0].clips[0];
    EXPECT_EQ(clip_after.name, "FinalName");
    EXPECT_EQ(clip_after.duration.ticks(), 9 * core::kTicksPerSecond);

    status = apply(p, inverse(cs));
    ASSERT_TRUE(test::is_ok(status));
    EXPECT_TRUE(model::identical(p, orig));
}

TEST(ApplyTest, NormalizeSortsUnsortedTrackClipsAndMarkers) {
    core::UuidGenerator gen(208ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId track_id = model::generate_id<model::TrackId>(gen);

    model::Clip c10;
    c10.id = model::generate_id<model::ClipId>(gen);
    c10.start = model::test::timeline_at_seconds(10);
    c10.duration = model::test::duration_of_seconds(1);

    model::Clip c0;
    c0.id = model::generate_id<model::ClipId>(gen);
    c0.start = model::TimelineTime::zero();
    c0.duration = model::test::duration_of_seconds(1);

    model::Clip c5;
    c5.id = model::generate_id<model::ClipId>(gen);
    c5.start = model::test::timeline_at_seconds(5);
    c5.duration = model::test::duration_of_seconds(1);

    model::Marker m10;
    m10.id = model::generate_id<model::MarkerId>(gen);
    m10.time = model::test::timeline_at_seconds(10);

    model::Marker m0;
    m0.id = model::generate_id<model::MarkerId>(gen);
    m0.time = model::TimelineTime::zero();

    model::Marker m5;
    m5.id = model::generate_id<model::MarkerId>(gen);
    m5.time = model::test::timeline_at_seconds(5);

    model::Project p;
    p.main_sequence = seq_id;
    model::Sequence seq;
    seq.id = seq_id;

    model::Track trk;
    trk.id = track_id;
    // Push clips in unsorted order: 10 s, 0 s, 5 s
    trk.clips.push_back(c10);
    trk.clips.push_back(c0);
    trk.clips.push_back(c5);

    seq.tracks.push_back(trk);
    // Push markers in unsorted order: 10 s, 0 s, 5 s
    seq.markers.push_back(m10);
    seq.markers.push_back(m0);
    seq.markers.push_back(m5);

    p.sequences[seq_id] = seq;

    normalize(p);

    const auto& sorted_clips = p.sequences[seq_id].tracks[0].clips;
    ASSERT_EQ(sorted_clips.size(), 3U);
    EXPECT_EQ(sorted_clips[0].id, c0.id);
    EXPECT_EQ(sorted_clips[1].id, c5.id);
    EXPECT_EQ(sorted_clips[2].id, c10.id);

    const auto& sorted_markers = p.sequences[seq_id].markers;
    ASSERT_EQ(sorted_markers.size(), 3U);
    EXPECT_EQ(sorted_markers[0].id, m0.id);
    EXPECT_EQ(sorted_markers[1].id, m5.id);
    EXPECT_EQ(sorted_markers[2].id, m10.id);
}

}  // namespace
}  // namespace nxtcut::commands
