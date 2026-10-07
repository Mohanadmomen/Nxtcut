#pragma once

#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/frame_time.hpp>
#include <nxtcut/core/result.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace nxtcut::core {

/**
 * @brief Mode determining whether timecode accounting drops frame numbers to compensate for drift.
 */
enum class DropFrameMode {
    NonDrop,  ///< Continuous integer numbering (separator ':').
    Drop,     ///< Drops frame labels to track wall-clock time (separator ';').
};

/**
 * @brief SMPTE timecode representation (hours, minutes, seconds, frames, drop-frame mode).
 *
 * @note Thread safety: Thread-safe (immutable value type).
 */
class Timecode {
public:
    constexpr Timecode() noexcept = default;

    constexpr Timecode(std::uint32_t hours, std::uint32_t minutes, std::uint32_t seconds,
                       std::uint32_t frames, DropFrameMode drop_mode) noexcept
        : hours_(hours),
          minutes_(minutes),
          seconds_(seconds),
          frames_(frames),
          drop_mode_(drop_mode) {}

    /**
     * @brief Computes timecode from a zero-based frame index, frame rate, and drop-frame mode.
     */
    [[nodiscard]] static Result<Timecode> from_frame(FrameIndex frame, FrameRate rate,
                                                     DropFrameMode drop_mode) noexcept;

    /**
     * @brief Converts this timecode into its equivalent zero-based frame index.
     */
    [[nodiscard]] Result<FrameIndex> to_frame(FrameRate rate) const noexcept;

    /**
     * @brief Formats this timecode as "HH:MM:SS:FF" (non-drop) or "HH:MM:SS;FF" (drop-frame).
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * @brief Parses a timecode string into a Timecode instance.
     *
     * Accepts ':' or ';' before frames. Rejects out-of-range fields or invalid drop-frame labels.
     */
    [[nodiscard]] static Result<Timecode> parse(std::string_view str, FrameRate rate);

    [[nodiscard]] constexpr std::uint32_t hours() const noexcept { return hours_; }

    [[nodiscard]] constexpr std::uint32_t minutes() const noexcept { return minutes_; }

    [[nodiscard]] constexpr std::uint32_t seconds() const noexcept { return seconds_; }

    [[nodiscard]] constexpr std::uint32_t frames() const noexcept { return frames_; }

    [[nodiscard]] constexpr DropFrameMode drop_frame_mode() const noexcept { return drop_mode_; }

    [[nodiscard]] constexpr bool is_drop_frame() const noexcept {
        return drop_mode_ == DropFrameMode::Drop;
    }

    [[nodiscard]] constexpr bool operator==(const Timecode&) const noexcept = default;

private:
    std::uint32_t hours_{0};
    std::uint32_t minutes_{0};
    std::uint32_t seconds_{0};
    std::uint32_t frames_{0};
    DropFrameMode drop_mode_{DropFrameMode::NonDrop};
};

}  // namespace nxtcut::core
