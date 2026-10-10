#include <nxtcut/keyframes/keyframe_track.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <vector>

namespace nxtcut::keyframes {
namespace {

struct DeterministicLcg {
    std::uint64_t state{123456789ULL};

    std::uint64_t next() noexcept {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }

    std::int64_t next_in_range(std::int64_t min_val, std::int64_t max_val) noexcept {
        const std::uint64_t span = static_cast<std::uint64_t>(max_val - min_val + 1);
        return min_val + static_cast<std::int64_t>(next() % span);
    }
};

KeyframeTrack<double> build_test_track_64() {
    std::vector<Keyframe<double>> keys;
    keys.reserve(64);

    const auto bezier = Interpolation::bezier(0.25, 0.1, 0.25, 1.0).value();

    for (std::size_t i = 0; i < 64; ++i) {
        Interpolation interp;
        switch (i % 5) {
            case 0:
                interp = Interpolation::linear();
                break;
            case 1:
                interp = Interpolation::hold();
                break;
            case 2:
                interp = Interpolation::easing(EasingKind::EaseInOutSine);
                break;
            case 3:
                interp = Interpolation::easing(EasingKind::EaseInQuad);
                break;
            case 4:
                interp = bezier;
                break;
        }

        const auto time = core::TimePoint::from_ticks(static_cast<std::int64_t>(i) * 100);
        const double value = static_cast<double>(i) * 1.5 + 2.0;
        keys.push_back(Keyframe<double>{time, value, interp});
    }

    return KeyframeTrack<double>::create(std::move(keys)).value();
}

TEST(KeyframeTrackCursorTest, CursorAndNonCursorBitIdentical100k) {
    const auto track = build_test_track_64();
    ASSERT_EQ(track.size(), 64U);

    DeterministicLcg lcg;
    constexpr std::size_t kProbeCount = 100000;
    std::vector<core::TimePoint> probe_times;
    probe_times.reserve(kProbeCount);

    // Range spanning before track (-500) to after track (7000)
    for (std::size_t i = 0; i < kProbeCount; ++i) {
        const std::int64_t tick = lcg.next_in_range(-500, 7000);
        probe_times.push_back(core::TimePoint::from_ticks(tick));
    }

    // 1. Forward sequential scan
    {
        auto forward_times = probe_times;
        std::sort(forward_times.begin(), forward_times.end());
        KeyframeTrack<double>::Cursor cursor;

        for (const auto t : forward_times) {
            const double direct = track.value_at(t);
            const double via_cursor = track.value_at(t, cursor);
            ASSERT_EQ(std::bit_cast<std::uint64_t>(direct),
                      std::bit_cast<std::uint64_t>(via_cursor));
        }
    }

    // 2. Backward sequential scan
    {
        auto backward_times = probe_times;
        std::sort(backward_times.begin(), backward_times.end(), std::greater<core::TimePoint>());
        KeyframeTrack<double>::Cursor cursor;

        for (const auto t : backward_times) {
            const double direct = track.value_at(t);
            const double via_cursor = track.value_at(t, cursor);
            ASSERT_EQ(std::bit_cast<std::uint64_t>(direct),
                      std::bit_cast<std::uint64_t>(via_cursor));
        }
    }

    // 3. Random jumps
    {
        KeyframeTrack<double>::Cursor cursor;
        for (const auto t : probe_times) {
            const double direct = track.value_at(t);
            const double via_cursor = track.value_at(t, cursor);
            ASSERT_EQ(std::bit_cast<std::uint64_t>(direct),
                      std::bit_cast<std::uint64_t>(via_cursor));
        }
    }

    // 4. Repeated same time
    {
        KeyframeTrack<double>::Cursor cursor;
        const std::array<std::int64_t, 5> test_ticks = {-100, 0, 3145, 6300, 8000};
        for (const auto tick : test_ticks) {
            const auto t = core::TimePoint::from_ticks(tick);
            const double direct = track.value_at(t);
            for (int r = 0; r < 1000; ++r) {
                const double via_cursor = track.value_at(t, cursor);
                ASSERT_EQ(std::bit_cast<std::uint64_t>(direct),
                          std::bit_cast<std::uint64_t>(via_cursor));
            }
        }
    }
}

TEST(KeyframeTrackCursorTest, StaleCursorRecovery) {
    auto track = build_test_track_64();

    // Outrageously out-of-range cursor
    KeyframeTrack<double>::Cursor stale_cursor{10000};
    const auto t = core::TimePoint::from_ticks(350);
    const double expected = track.value_at(t);
    const double recovered = track.value_at(t, stale_cursor);

    EXPECT_EQ(std::bit_cast<std::uint64_t>(expected), std::bit_cast<std::uint64_t>(recovered));
    EXPECT_LT(stale_cursor.segment, track.size());

    // Shrink track and test stale cursor again
    stale_cursor.segment = 10000;
    while (track.size() > 5) {
        static_cast<void>(track.remove_at(track.keys().back().time));
    }
    const auto t_shrunk = core::TimePoint::from_ticks(150);
    const double expected_shrunk = track.value_at(t_shrunk);
    const double recovered_shrunk = track.value_at(t_shrunk, stale_cursor);

    EXPECT_EQ(std::bit_cast<std::uint64_t>(expected_shrunk),
              std::bit_cast<std::uint64_t>(recovered_shrunk));
    EXPECT_LT(stale_cursor.segment, track.size());
}

}  // namespace
}  // namespace nxtcut::keyframes
