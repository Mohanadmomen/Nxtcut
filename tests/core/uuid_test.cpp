#include <nxtcut/core/uuid.hpp>

#include <gtest/gtest.h>

#include <thread>
#include <unordered_set>
#include <vector>

namespace nxtcut::core {
namespace {

TEST(UuidTest, NilUuid) {
    const Uuid nil_id = Uuid::nil();
    EXPECT_TRUE(nil_id.is_nil());
    EXPECT_EQ(nil_id.to_string(), "00000000-0000-0000-0000-000000000000");

    const Uuid default_id{};
    EXPECT_TRUE(default_id.is_nil());
    EXPECT_EQ(nil_id, default_id);
}

TEST(UuidTest, ParseAndFormatRoundTrip) {
    const std::string canonical = "f47ac10b-58cc-4372-a567-0e02b2c3d479";
    const auto parsed = Uuid::parse(canonical);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->is_nil());
    EXPECT_EQ(parsed->to_string(), canonical);

    // Case-insensitivity test
    const std::string upper = "F47AC10B-58CC-4372-A567-0E02B2C3D479";
    const auto parsed_upper = Uuid::parse(upper);
    ASSERT_TRUE(parsed_upper.has_value());
    EXPECT_EQ(*parsed, *parsed_upper);
    EXPECT_EQ(parsed_upper->to_string(), canonical);
}

TEST(UuidTest, ParseRejectsInvalidStrings) {
    EXPECT_FALSE(Uuid::parse("").has_value());
    EXPECT_FALSE(Uuid::parse("short-uuid").has_value());
    EXPECT_FALSE(Uuid::parse("f47ac10b-58cc-4372-a567-0e02b2c3d47").has_value());
    EXPECT_FALSE(Uuid::parse("f47ac10b-58cc-4372-a567-0e02b2c3d479a").has_value());
    EXPECT_FALSE(Uuid::parse("f47ac10b_58cc_4372_a567_0e02b2c3d479").has_value());
    EXPECT_FALSE(Uuid::parse("g47ac10b-58cc-4372-a567-0e02b2c3d479").has_value());
    EXPECT_FALSE(Uuid::parse("f47ac10z-58cc-4372-a567-0e02b2c3d479").has_value());
}

TEST(UuidTest, GeneratorDeterministicSeeding) {
    UuidGenerator gen1(1337ULL);
    UuidGenerator gen2(1337ULL);

    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(gen1.generate(), gen2.generate());
    }

    UuidGenerator gen3(9999ULL);
    EXPECT_NE(gen1.generate(), gen3.generate());
}

TEST(UuidTest, GeneratorProducesValidV4Bits) {
    UuidGenerator gen(42ULL);

    for (int i = 0; i < 100; ++i) {
        const Uuid u = gen.generate();
        EXPECT_FALSE(u.is_nil());

        // Version 4: high 4 bits of byte 6 must be 0100 (4)
        const std::uint8_t version = static_cast<std::uint8_t>(u.bytes()[6] >> 4);
        EXPECT_EQ(version, 4);

        // Variant 1: high 2 bits of byte 8 must be 10 (2)
        const std::uint8_t variant = static_cast<std::uint8_t>(u.bytes()[8] >> 6);
        EXPECT_EQ(variant, 2);
    }
}

TEST(UuidTest, StdHashSupport) {
    std::unordered_set<Uuid> set;
    UuidGenerator gen(123ULL);

    for (int i = 0; i < 100; ++i) {
        set.insert(gen.generate());
    }
    EXPECT_EQ(set.size(), 100U);
}

TEST(UuidTest, ThreadSafeGeneration) {
    UuidGenerator gen(555ULL);
    constexpr int kNumThreads = 4;
    constexpr int kUuidsPerThread = 500;

    std::vector<std::vector<Uuid>> per_thread_uuids(kNumThreads);
    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&gen, &per_thread_uuids, t]() {
            per_thread_uuids[static_cast<std::size_t>(t)].reserve(kUuidsPerThread);
            for (int i = 0; i < kUuidsPerThread; ++i) {
                per_thread_uuids[static_cast<std::size_t>(t)].push_back(gen.generate());
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    std::unordered_set<Uuid> all_uuids;
    for (const auto& vec : per_thread_uuids) {
        for (const auto& u : vec) {
            all_uuids.insert(u);
        }
    }

    EXPECT_EQ(all_uuids.size(), static_cast<std::size_t>(kNumThreads * kUuidsPerThread));
}

}  // namespace
}  // namespace nxtcut::core
