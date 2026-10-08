#include <nxtcut/model/project.hpp>

#include <gtest/gtest.h>

#include "test_fixtures.hpp"

namespace nxtcut::model {
namespace {

TEST(ProjectTest, LookupsFoundAndNotFound) {
    const Project project = test::build_valid_project(12345ULL);

    // Lookups found
    const MediaAsset* video_asset = find_media(project, project.media.begin()->first);
    ASSERT_NE(video_asset, nullptr);
    EXPECT_EQ(video_asset->id, project.media.begin()->first);

    const Sequence* main_seq = find_sequence(project, project.main_sequence);
    ASSERT_NE(main_seq, nullptr);
    EXPECT_EQ(main_seq->id, project.main_sequence);

    // Lookups not found
    core::UuidGenerator gen(999ULL);
    const MediaId missing_mid = generate_id<MediaId>(gen);
    const SequenceId missing_sid = generate_id<SequenceId>(gen);

    EXPECT_EQ(find_media(project, missing_mid), nullptr);
    EXPECT_EQ(find_sequence(project, missing_sid), nullptr);
}

TEST(ProjectTest, SnapshotSemantics) {
    const Project original = test::build_valid_project(54321ULL);
    Project snapshot = original;

    // Mutate snapshot
    snapshot.name = "Modified Snapshot Name";
    snapshot.sequences[snapshot.main_sequence].name = "Renamed Sequence";
    snapshot.media.clear();

    // Verify original remains strictly unchanged
    EXPECT_EQ(original.name, "SampleProject");
    EXPECT_EQ(original.sequences.at(original.main_sequence).name, "MainSequence");
    EXPECT_EQ(original.media.size(), 2U);
}

}  // namespace
}  // namespace nxtcut::model
