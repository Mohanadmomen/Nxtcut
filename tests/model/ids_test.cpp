#include <nxtcut/model/ids.hpp>

#include <gtest/gtest.h>

#include <map>
#include <unordered_set>
#include <vector>

#include "compile_checks.hpp"

namespace nxtcut::model {
namespace {

using test::CanConstruct;
using test::CanConvert;
using test::CanEqual;
using test::CanOrder;

// Static assertions verifying strong typing
static_assert(!CanConvert<ClipId, TrackId>);
static_assert(!CanConvert<TrackId, ClipId>);
static_assert(!CanConvert<ProjectId, SequenceId>);
static_assert(!CanConvert<MediaId, EffectId>);
static_assert(!CanConvert<MarkerId, LinkId>);

static_assert(!CanConstruct<ClipId, TrackId>);
static_assert(!CanConstruct<TrackId, ClipId>);
static_assert(!CanConstruct<ProjectId, SequenceId>);
static_assert(!CanConstruct<MediaId, EffectId>);

static_assert(!CanEqual<ClipId, TrackId>);
static_assert(!CanOrder<ClipId, TrackId>);

static_assert(CanEqual<ClipId, ClipId>);
static_assert(CanOrder<ClipId, ClipId>);
static_assert(CanEqual<TrackId, TrackId>);
static_assert(CanOrder<TrackId, TrackId>);
static_assert(CanEqual<ProjectId, ProjectId>);
static_assert(CanOrder<ProjectId, ProjectId>);

TEST(IdsTest, DefaultAndNilConstruction) {
    const ClipId default_clip_id;
    EXPECT_TRUE(default_clip_id.is_nil());
    EXPECT_EQ(default_clip_id, ClipId::nil());

    const TrackId default_track_id;
    EXPECT_TRUE(default_track_id.is_nil());
}

TEST(IdsTest, GenerationAndDistinctness) {
    core::UuidGenerator gen(12345ULL);
    std::unordered_set<ClipId> seen;
    seen.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        const ClipId id = generate_id<ClipId>(gen);
        EXPECT_FALSE(id.is_nil());
        EXPECT_TRUE(seen.insert(id).second) << "Duplicate ID generated at index " << i;
    }

    EXPECT_EQ(seen.size(), 1000U);
}

TEST(IdsTest, ReproducibilityWithSameSeed) {
    core::UuidGenerator gen1(987654321ULL);
    core::UuidGenerator gen2(987654321ULL);

    for (int i = 0; i < 100; ++i) {
        const TrackId id1 = generate_id<TrackId>(gen1);
        const TrackId id2 = generate_id<TrackId>(gen2);
        EXPECT_EQ(id1, id2);
    }
}

TEST(IdsTest, UsableAsStdMapKeys) {
    core::UuidGenerator gen(42ULL);
    std::map<ProjectId, std::string> project_map;
    std::vector<ProjectId> keys;

    for (int i = 0; i < 50; ++i) {
        const ProjectId id = generate_id<ProjectId>(gen);
        keys.push_back(id);
        project_map[id] = "Project_" + std::to_string(i);
    }

    EXPECT_EQ(project_map.size(), 50U);

    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto it = project_map.find(keys[i]);
        ASSERT_NE(it, project_map.end());
        EXPECT_EQ(it->second, "Project_" + std::to_string(i));
    }
}

}  // namespace
}  // namespace nxtcut::model
