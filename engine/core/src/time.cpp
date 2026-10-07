#include <nxtcut/core/time.hpp>

namespace nxtcut::core {

Result<TimeRange> TimeRange::create(TimePoint start, Duration d) {
    if (d.ticks() < 0) {
        return make_error(ErrorCode::InvalidArgument, "TimeRange duration cannot be negative");
    }
    return TimeRange(start, d);
}

Result<TimeRange> TimeRange::from_start_end(TimePoint start, TimePoint end) {
    if (end < start) {
        return make_error(ErrorCode::InvalidArgument,
                          "TimeRange end point must not precede start point");
    }
    return TimeRange(start, end - start);
}

}  // namespace nxtcut::core
