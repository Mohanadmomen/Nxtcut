#include <nxtcut/core/frame_rate.hpp>

#include <fmt/format.h>

#include <numeric>

namespace nxtcut::core {

Result<FrameRate> FrameRate::create(std::int32_t num, std::int32_t den) {
    if (num <= 0 || den <= 0) {
        return make_error(ErrorCode::InvalidArgument,
                          "FrameRate numerator and denominator must both be positive integers");
    }
    const std::int32_t divisor = std::gcd(num, den);
    return FrameRateInternalAccess::make(num / divisor, den / divisor);
}

std::string FrameRate::to_string() const {
    if (denominator_ == 1) {
        return fmt::format("{}", numerator_);
    }
    if (denominator_ == 1001) {
        if (numerator_ == 24000) {
            return "23.976";
        }
        if (numerator_ == 30000) {
            return "29.97";
        }
        if (numerator_ == 60000) {
            return "59.94";
        }
    }
    if (numerator_ % denominator_ == 0) {
        return fmt::format("{}", numerator_ / denominator_);
    }
    return fmt::format("{:.2f}", to_double());
}

Result<SampleRate> SampleRate::create(std::int32_t value) {
    if (value <= 0) {
        return make_error(ErrorCode::InvalidArgument, "SampleRate must be a positive integer");
    }
    return SampleRateInternalAccess::make(value);
}

}  // namespace nxtcut::core
