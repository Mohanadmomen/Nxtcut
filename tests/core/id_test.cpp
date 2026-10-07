#include <nxtcut/core/id.hpp>
#include <nxtcut/core/uuid.hpp>

#include <gtest/gtest.h>

#include <type_traits>
#include <unordered_set>

#include "compile_checks.hpp"

namespace nxtcut::core {
namespace {

using test::CanEqual;
using test::CanOrder;

struct ClipTag {};
struct TrackTag {};

using ClipId = Id<ClipTag>;
using TrackId = Id<TrackTag>;

// Compile-time static assertions proving strong type distinction between Id<A> and Id<B>
static_assert(!std::is_convertible_v<ClipId, TrackId>,
              "Id<A> must not implicitly convert to Id<B>");
static_assert(!std::is_convertible_v<TrackId, ClipId>,
              "Id<B> must not implicitly convert to Id<A>");
static_assert(!std::is_constructible_v<ClipId, TrackId>,
              "Id<A> must not be constructible from Id<B>");
static_assert(!std::is_assignable_v<ClipId&, TrackId>, "Id<A> must not be assignable from Id<B>");
static_assert(!CanEqual<ClipId, TrackId>);
static_assert(!CanOrder<ClipId, TrackId>);
static_assert(CanEqual<ClipId, ClipId>);
static_assert(CanOrder<ClipId, ClipId>);
static_assert(CanEqual<TrackId, TrackId>);
static_assert(CanOrder<TrackId, TrackId>);

TEST(IdTest, DefaultAndNilConstruction) {
    const ClipId default_id{};
    EXPECT_TRUE(default_id.is_nil());
    EXPECT_EQ(default_id.uuid(), Uuid::nil());
    EXPECT_EQ(default_id.to_string(), "00000000-0000-0000-0000-000000000000");

    const ClipId nil_id = ClipId::nil();
    EXPECT_TRUE(nil_id.is_nil());
    EXPECT_EQ(default_id, nil_id);
}

TEST(IdTest, ExplicitConstructionFromUuid) {
    UuidGenerator gen(12345ULL);
    const Uuid u = gen.generate();

    const ClipId cid(u);
    EXPECT_FALSE(cid.is_nil());
    EXPECT_EQ(cid.uuid(), u);
    EXPECT_EQ(cid.to_string(), u.to_string());
}

TEST(IdTest, EqualityAndOrdering) {
    UuidGenerator gen(999ULL);
    const ClipId id1(gen.generate());
    const ClipId id2(gen.generate());
    const ClipId id1_copy = id1;

    EXPECT_EQ(id1, id1_copy);
    EXPECT_NE(id1, id2);

    if (id1 < id2) {
        EXPECT_LE(id1, id2);
        EXPECT_GT(id2, id1);
        EXPECT_GE(id2, id1);
    } else {
        EXPECT_LE(id2, id1);
        EXPECT_GT(id1, id2);
        EXPECT_GE(id1, id2);
    }
}

TEST(IdTest, StdHashSupportInContainers) {
    UuidGenerator gen(777ULL);
    std::unordered_set<ClipId> set;

    for (int i = 0; i < 50; ++i) {
        set.insert(ClipId(gen.generate()));
    }

    EXPECT_EQ(set.size(), 50U);
}

}  // namespace
}  // namespace nxtcut::core
