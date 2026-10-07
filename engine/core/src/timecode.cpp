#include <nxtcut/core/timecode.hpp>

#include <fmt/format.h>

namespace nxtcut::core {

Result<Timecode> Timecode::from_frame(
    FrameIndex frame,
    FrameRate rate,
    DropFrameMode drop_mode
) noexcept {
    if (frame.value() < 0) {
        return make_error(ErrorCode::OutOfRange, "Timecode frame index must be non-negative");
    }

    const std::int32_t nominal = rate.nominal_fps();
    if (nominal <= 0) {
        return make_error(ErrorCode::InvalidArgument, "Invalid nominal fps");
    }

    if (drop_mode == DropFrameMode::Drop) {
        std::int64_t drop_count = 0;
        if (rate.numerator() == 30000 && rate.denominator() == 1001) {
            drop_count = 2;
        } else if (rate.numerator() == 60000 && rate.denominator() == 1001) {
            drop_count = 4;
        } else {
            return make_error(
                ErrorCode::Unsupported,
                "Drop-frame timecode is only supported for 29.97 (30000/1001) and 59.94 (60000/1001) fps"
            );
        }

        const std::int64_t frames_per_10min = 10 * 60 * static_cast<std::int64_t>(nominal) - 9 * drop_count;
        const std::int64_t frames_per_min = 60 * static_cast<std::int64_t>(nominal) - drop_count;
        const std::int64_t nominal_frames_min0 = 60 * static_cast<std::int64_t>(nominal);

        const std::int64_t f = frame.value();
        const std::int64_t d10 = f / frames_per_10min;
        const std::int64_t m10 = f % frames_per_10min;

        std::int64_t min_idx = 0;
        if (m10 >= nominal_frames_min0) {
            min_idx = 1 + (m10 - nominal_frames_min0) / frames_per_min;
        }

        const std::int64_t total_dropped = d10 * (9 * drop_count) + min_idx * drop_count;
        const std::int64_t nom_frame = f + total_dropped;

        const std::uint32_t ff = static_cast<std::uint32_t>(nom_frame % nominal);
        const std::int64_t total_sec = nom_frame / nominal;
        const std::uint32_t ss = static_cast<std::uint32_t>(total_sec % 60);
        const std::int64_t total_min = total_sec / 60;
        const std::uint32_t mm = static_cast<std::uint32_t>(total_min % 60);
        const std::uint32_t hh = static_cast<std::uint32_t>(total_min / 60);

        return Timecode(hh, mm, ss, ff, DropFrameMode::Drop);
    } else {
        const std::int64_t f = frame.value();
        const std::uint32_t ff = static_cast<std::uint32_t>(f % nominal);
        const std::int64_t total_sec = f / nominal;
        const std::uint32_t ss = static_cast<std::uint32_t>(total_sec % 60);
        const std::int64_t total_min = total_sec / 60;
        const std::uint32_t mm = static_cast<std::uint32_t>(total_min % 60);
        const std::uint32_t hh = static_cast<std::uint32_t>(total_min / 60);

        return Timecode(hh, mm, ss, ff, DropFrameMode::NonDrop);
    }
}

Result<FrameIndex> Timecode::to_frame(FrameRate rate) const noexcept {
    const std::int32_t nominal = rate.nominal_fps();
    if (nominal <= 0) {
        return make_error(ErrorCode::InvalidArgument, "Invalid nominal fps");
    }

    if (frames_ >= static_cast<std::uint32_t>(nominal)) {
        return make_error(ErrorCode::InvalidArgument, "Timecode frames field exceeds nominal fps");
    }
    if (minutes_ >= 60 || seconds_ >= 60) {
        return make_error(ErrorCode::InvalidArgument, "Timecode minutes or seconds field exceeds 59");
    }

    if (drop_mode_ == DropFrameMode::Drop) {
        std::int64_t drop_count = 0;
        if (rate.numerator() == 30000 && rate.denominator() == 1001) {
            drop_count = 2;
        } else if (rate.numerator() == 60000 && rate.denominator() == 1001) {
            drop_count = 4;
        } else {
            return make_error(
                ErrorCode::Unsupported,
                "Drop-frame timecode is only supported for 29.97 (30000/1001) and 59.94 (60000/1001) fps"
            );
        }

        if ((minutes_ % 10 != 0) && (seconds_ == 0) && (frames_ < static_cast<std::uint32_t>(drop_count))) {
            return make_error(
                ErrorCode::InvalidArgument,
                "Timecode refers to a dropped frame label that does not exist in drop-frame mode"
            );
        }

        const std::int64_t total_minutes = static_cast<std::int64_t>(hours_) * 60 + minutes_;
        const std::int64_t nominal_frames =
            ((total_minutes * 60) + seconds_) * nominal + frames_;
        const std::int64_t dropped_frames =
            (total_minutes / 10) * (9 * drop_count) + (total_minutes % 10) * drop_count;

        return FrameIndex(nominal_frames - dropped_frames);
    } else {
        const std::int64_t total_minutes = static_cast<std::int64_t>(hours_) * 60 + minutes_;
        const std::int64_t total_frames =
            ((total_minutes * 60) + seconds_) * nominal + frames_;
        return FrameIndex(total_frames);
    }
}

std::string Timecode::to_string() const {
    const char sep = (drop_mode_ == DropFrameMode::Drop) ? ';' : ':';
    return fmt::format("{:02d}:{:02d}:{:02d}{}{:02d}", hours_, minutes_, seconds_, sep, frames_);
}

Result<Timecode> Timecode::parse(std::string_view str, FrameRate rate) {
    if (str.size() != 11) {
        return make_error(
            ErrorCode::InvalidArgument,
            "Timecode string must be 11 characters (HH:MM:SS:FF or HH:MM:SS;FF)"
        );
    }
    if (str[2] != ':' || str[5] != ':') {
        return make_error(ErrorCode::InvalidArgument, "Invalid delimiter in timecode string");
    }
    const char sep = str[8];
    if (sep != ':' && sep != ';') {
        return make_error(ErrorCode::InvalidArgument, "Invalid frame separator in timecode string");
    }
    const DropFrameMode mode = (sep == ';') ? DropFrameMode::Drop : DropFrameMode::NonDrop;

    auto parse2digits = [](char c1, char c2, std::uint32_t& out) -> bool {
        if (c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9') {
            return false;
        }
        out = static_cast<std::uint32_t>((c1 - '0') * 10 + (c2 - '0'));
        return true;
    };

    std::uint32_t hh = 0;
    std::uint32_t mm = 0;
    std::uint32_t ss = 0;
    std::uint32_t ff = 0;
    if (!parse2digits(str[0], str[1], hh) ||
        !parse2digits(str[3], str[4], mm) ||
        !parse2digits(str[6], str[7], ss) ||
        !parse2digits(str[9], str[10], ff)) {
        return make_error(ErrorCode::InvalidArgument, "Timecode fields must contain decimal digits");
    }

    if (mm >= 60 || ss >= 60) {
        return make_error(ErrorCode::InvalidArgument, "Timecode minutes or seconds out of range [0, 59]");
    }

    const std::int32_t nominal = rate.nominal_fps();
    if (ff >= static_cast<std::uint32_t>(nominal)) {
        return make_error(ErrorCode::InvalidArgument, "Timecode frames out of range for rate nominal fps");
    }

    if (mode == DropFrameMode::Drop) {
        std::int64_t drop_count = 0;
        if (rate.numerator() == 30000 && rate.denominator() == 1001) {
            drop_count = 2;
        } else if (rate.numerator() == 60000 && rate.denominator() == 1001) {
            drop_count = 4;
        } else {
            return make_error(
                ErrorCode::Unsupported,
                "Drop-frame is only supported for 29.97 (30000/1001) and 59.94 (60000/1001) fps"
            );
        }

        if ((mm % 10 != 0) && (ss == 0) && (ff < static_cast<std::uint32_t>(drop_count))) {
            return make_error(
                ErrorCode::InvalidArgument,
                "Timecode specifies a dropped frame label that does not exist in drop-frame mode"
            );
        }
    }

    return Timecode(hh, mm, ss, ff, mode);
}

}  // namespace nxtcut::core
