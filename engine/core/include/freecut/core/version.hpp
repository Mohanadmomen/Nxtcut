#pragma once

#include <cstdint>
#include <string>

namespace freecut::core {

/**
 * @brief Semantic version representation {major, minor, patch} for FreeCut engine.
 *
 * @note Thread safety: Thread-safe (immutable data type with value semantics).
 */
struct Version {
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::uint32_t patch{0};

    /**
     * @brief Three-way comparison operator for semantic ordering and equality.
     */
    [[nodiscard]] constexpr auto operator<=>(const Version&) const noexcept = default;

    /**
     * @brief Formats this Version as a string "major.minor.patch".
     *
     * @return Formatted semantic version string.
     */
    [[nodiscard]] std::string to_string() const;
};

/**
 * @brief Free function helper to format a Version as a string.
 *
 * @param ver Version structure to convert.
 * @return Formatted semantic version string "major.minor.patch".
 *
 * @note Thread safety: Thread-safe (re-entrant pure function).
 */
[[nodiscard]] std::string to_string(const Version& ver);

/**
 * @brief Retrieves the current version of the FreeCut engine.
 *
 * @return Engine version structure containing major, minor, and patch numbers.
 *
 * @note Thread safety: Thread-safe (re-entrant, returns fixed compile-time constants).
 */
[[nodiscard]] Version version() noexcept;

}  // namespace freecut::core
