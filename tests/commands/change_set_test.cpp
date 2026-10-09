#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/track.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace nxtcut::commands {
namespace {

TEST(ChangeSetTest, EmptyChangeSetReportsEmpty) {
    ChangeSet cs;
    EXPECT_TRUE(cs.empty());
    cs.label = "Sample";
    EXPECT_TRUE(cs.empty());
}

TEST(ChangeSetTest, InverseReversesOrderAndSwapsSides) {
    core::UuidGenerator gen(101ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId trk_id = model::generate_id<model::TrackId>(gen);
    const model::ClipId clip_id = model::generate_id<model::ClipId>(gen);

    ChangeSet cs;
    cs.label = "TestBatch";

    // Change 1: TrackMove from 1 to 3
    cs.changes.push_back(TrackMove{seq_id, trk_id, 1, 3});

    // Change 2: ClipChange modification
    model::Clip c1;
    c1.id = clip_id;
    c1.name = "BeforeClip";
    model::Clip c2 = c1;
    c2.name = "AfterClip";
    cs.changes.push_back(ClipChange{seq_id, trk_id, clip_id, c1, c2});

    ChangeSet inv = inverse(cs);
    EXPECT_EQ(inv.label, cs.label);
    ASSERT_EQ(inv.changes.size(), 2U);

    // Inverse Change 0 must be ClipChange with sides swapped
    ASSERT_TRUE(std::holds_alternative<ClipChange>(inv.changes[0]));
    const auto& inv_clip = std::get<ClipChange>(inv.changes[0]);
    EXPECT_EQ(inv_clip.sequence, seq_id);
    EXPECT_EQ(inv_clip.track, trk_id);
    EXPECT_EQ(inv_clip.id, clip_id);
    ASSERT_TRUE(inv_clip.before.has_value());
    ASSERT_TRUE(inv_clip.after.has_value());
    EXPECT_EQ(inv_clip.before->name, "AfterClip");
    EXPECT_EQ(inv_clip.after->name, "BeforeClip");

    // Inverse Change 1 must be TrackMove with from/to indices swapped
    ASSERT_TRUE(std::holds_alternative<TrackMove>(inv.changes[1]));
    const auto& inv_move = std::get<TrackMove>(inv.changes[1]);
    EXPECT_EQ(inv_move.sequence, seq_id);
    EXPECT_EQ(inv_move.id, trk_id);
    EXPECT_EQ(inv_move.from_index, 3U);
    EXPECT_EQ(inv_move.to_index, 1U);
}

TEST(ChangeSetTest, DoubleInverseRestoresOriginalStructure) {
    core::UuidGenerator gen(102ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId trk_id = model::generate_id<model::TrackId>(gen);

    ChangeSet cs;
    cs.label = "DoubleInv";
    cs.changes.push_back(TrackMove{seq_id, trk_id, 0, 2});
    cs.changes.push_back(TrackPropertiesChange{seq_id, trk_id, TrackProperties{"Old", true, false},
                                               TrackProperties{"New", false, true}});

    ChangeSet inv = inverse(cs);
    ChangeSet double_inv = inverse(inv);

    EXPECT_EQ(double_inv.label, cs.label);
    ASSERT_EQ(double_inv.changes.size(), 2U);

    ASSERT_TRUE(std::holds_alternative<TrackMove>(double_inv.changes[0]));
    const auto& move = std::get<TrackMove>(double_inv.changes[0]);
    EXPECT_EQ(move.from_index, 0U);
    EXPECT_EQ(move.to_index, 2U);

    ASSERT_TRUE(std::holds_alternative<TrackPropertiesChange>(double_inv.changes[1]));
    const auto& props = std::get<TrackPropertiesChange>(double_inv.changes[1]);
    EXPECT_EQ(props.before.name, "Old");
    EXPECT_EQ(props.after.name, "New");
}

TEST(ChangeSetTest, ReceiptOfListsOnlyCreationsInOrder) {
    core::UuidGenerator gen(103ULL);
    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(gen);
    const model::TrackId trk_id = model::generate_id<model::TrackId>(gen);
    const model::ClipId clip_id = model::generate_id<model::ClipId>(gen);
    const model::MarkerId marker_id = model::generate_id<model::MarkerId>(gen);
    const model::MediaId media_id = model::generate_id<model::MediaId>(gen);

    ChangeSet cs;
    cs.label = "CreateEntities";

    // 1. Add Sequence
    model::Sequence s;
    s.id = seq_id;
    cs.changes.push_back(SequenceChange{seq_id, std::nullopt, s});

    // 2. Add Track
    model::Track t;
    t.id = trk_id;
    cs.changes.push_back(TrackChange{seq_id, trk_id, std::nullopt, TrackSlot{0, t}});

    // 3. Add Clip
    model::Clip c;
    c.id = clip_id;
    cs.changes.push_back(ClipChange{seq_id, trk_id, clip_id, std::nullopt, c});

    // 4. Modify Clip (should NOT appear in receipt)
    model::Clip modified_c = c;
    modified_c.name = "Renamed";
    cs.changes.push_back(ClipChange{seq_id, trk_id, clip_id, c, modified_c});

    // 5. Add Marker
    model::Marker m;
    m.id = marker_id;
    cs.changes.push_back(MarkerChange{seq_id, marker_id, std::nullopt, m});

    // 6. Add Media
    model::MediaAsset asset;
    asset.id = media_id;
    cs.changes.push_back(MediaChange{media_id, std::nullopt, asset});

    EditReceipt receipt = receipt_of(cs);
    EXPECT_EQ(receipt.created_sequences, std::vector<model::SequenceId>{seq_id});
    EXPECT_EQ(receipt.created_tracks, std::vector<model::TrackId>{trk_id});
    EXPECT_EQ(receipt.created_clips, std::vector<model::ClipId>{clip_id});
    EXPECT_EQ(receipt.created_markers, std::vector<model::MarkerId>{marker_id});
    EXPECT_EQ(receipt.created_media, std::vector<model::MediaId>{media_id});
}

}  // namespace
}  // namespace nxtcut::commands
