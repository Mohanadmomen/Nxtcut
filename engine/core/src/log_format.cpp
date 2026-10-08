#include <nxtcut/core/log_format.hpp>

#include <fmt/format.h>

namespace nxtcut::core {
namespace {

constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t res = a / b;
    std::int64_t rem = a % b;
    if (rem != 0 && ((a < 0) ^ (b < 0))) {
        res -= 1;
    }
    return res;
}

constexpr std::int64_t floor_mod(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t rem = a % b;
    if (rem != 0 && ((a < 0) ^ (b < 0))) {
        rem += b;
    }
    return rem;
}

struct CivilDate {
    std::int64_t year{0};
    unsigned int month{0};
    unsigned int day{0};
};

constexpr CivilDate civil_from_days(std::int64_t z) noexcept {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = static_cast<unsigned int>(z - era * 146097);
    const unsigned int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned int mp = (5 * doy + 2) / 153;
    const unsigned int d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned int m = mp < 10 ? mp + 3 : mp - 9;
    return CivilDate{y + (m <= 2 ? 1 : 0), m, d};
}

}  // namespace

std::string format_log_line(const LogRecord& record) {
    const std::int64_t total_sec = floor_div(record.unix_millis, 1000);
    const std::int64_t millis = floor_mod(record.unix_millis, 1000);

    const std::int64_t days = floor_div(total_sec, 86400);
    const std::int64_t sec_of_day = floor_mod(total_sec, 86400);

    const std::int64_t hours = sec_of_day / 3600;
    const std::int64_t minutes = (sec_of_day % 3600) / 60;
    const std::int64_t seconds = sec_of_day % 60;

    const CivilDate date = civil_from_days(days);

    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d} {} [{}] {}", date.year,
                       date.month, date.day, hours, minutes, seconds, millis,
                       to_string(record.level), record.logger_name, record.message);
}

}  // namespace nxtcut::core
