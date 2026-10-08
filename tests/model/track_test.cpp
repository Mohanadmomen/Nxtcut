#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/track.hpp>

#include <gtest/gtest.h>

namespace nxtcut::model {
namespace {

TEST(TrackTest, TrackKindToString) {
    EXPECT_EQ(to_string(TrackKind::Video), "Video");
    EXPECT_EQ(to_string(TrackKind::Audio), "Audio");
}

TEST(TrackTest, TrackDefaults) {
    Track track;
    EXPECT_TRUE(track.enabled);
    EXPECT_FALSE(track.locked);
    EXPECT_EQ(track.kind, TrackKind::Video);
    EXPECT_TRUE(track.clips.empty());
}

TEST(TrackTest, FindTrackAndClip) {
    core::UuidGenerator gen(9999ULL);
    const SequenceId seq_id = generate_id<SequenceId>(gen);
    const TrackId t1_id = generate_id<TrackId>(gen);
    const TrackId t2_id = generate_id<TrackId>(gen);
    const TrackId missing_track = generate_id<TrackId>(gen);
    const ClipId c1_id = generate_id<ClipId>(gen);
    const ClipId missing_clip = generate_id<ClipId>(gen);

    Sequence seq;
    seq.id = seq_id;

    Track t1;
    t1.id = t1_id;
    t1.name = "Track1";

    Clip c1;
    c1.id = c1_id;
    c1.name = "Clip1";
    t1.clips.push_back(c1);

    Track t2;
    t2.id = t2_id;
    t2.name = "Track2";

    seq.tracks.push_back(t1);
    seq.tracks.push_back(t2);

    // Lookups found
    const Track* found_t1 = find_track(seq, t1_id);
    ASSERT_NE(found_t1, nullptr);
    EXPECT_EQ(found_t1->name, "Track1");

    const Track* found_t2 = find_track(seq, t2_id);
    ASSERT_NE(found_t2, nullptr);
    EXPECT_EQ(found_t2->name, "Track2");

    const Clip* found_c1 = find_clip(seq, c1_id);
    ASSERT_NE(found_c1, nullptr);
    EXPECT_EQ(found_c1->name, "Clip1");

    // Lookups not found
    EXPECT_EQ(find_track(seq, missing_track), nullptr);
    EXPECT_EQ(find_clip(seq, missing_clip), nullptr);
}

}  // namespace
}  // namespace nxtcut::model
