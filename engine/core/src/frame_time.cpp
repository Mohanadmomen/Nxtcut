#include <nxtcut/core/frame_time.hpp>

namespace nxtcut::core {

Result<TimePoint> frame_to_time(FrameIndex frame, FrameRate rate) noexcept {
    const std::int64_t b = static_cast<std::int64_t>(rate.denominator()) * kTicksPerSecond;
    const auto res = mul_div(frame.value(), b, rate.numerator(), RoundingMode::Nearest);
    if (!res) {
        return tl::unexpected(res.error());
    }
    return TimePoint::from_ticks(*res);
}

Result<FrameIndex> time_to_frame(
    TimePoint time,
    FrameRate rate,
    RoundingMode mode
) noexcept {
    const std::int64_t c = static_cast<std::int64_t>(rate.denominator()) * kTicksPerSecond;
    const auto res = mul_div(time.ticks(), rate.numerator(), c, mode);
    if (!res) {
        return tl::unexpected(res.error());
    }
    return FrameIndex(*res);
}

Result<Duration> frame_duration(FrameRate rate) noexcept {
    const std::int64_t b = static_cast<std::int64_t>(rate.denominator()) * kTicksPerSecond;
    const auto res = mul_div(1, b, rate.numerator(), RoundingMode::Nearest);
    if (!res) {
        return tl::unexpected(res.error());
    }
    return Duration::from_ticks(*res);
}

Result<TimePoint> snap_to_frame(
    TimePoint time,
    FrameRate rate,
    RoundingMode mode
) noexcept {
    const auto frame_res = time_to_frame(time, rate, mode);
    if (!frame_res) {
        return tl::unexpected(frame_res.error());
    }
    return frame_to_time(*frame_res, rate);
}

Result<TimePoint> samples_to_time(
    std::int64_t samples,
    SampleRate rate
) noexcept {
    const auto res = mul_div(samples, kTicksPerSecond, rate.value(), RoundingMode::Nearest);
    if (!res) {
        return tl::unexpected(res.error());
    }
    return TimePoint::from_ticks(*res);
}

Result<std::int64_t> time_to_samples(
    TimePoint time,
    SampleRate rate,
    RoundingMode mode
) noexcept {
    return mul_div(time.ticks(), rate.value(), kTicksPerSecond, mode);
}

}  // namespace nxtcut::core
