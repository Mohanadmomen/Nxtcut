#pragma once

#include <nxtcut/core/result.hpp>

#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <string_view>

namespace nxtcut::core {

/**
 * @brief Represents a 128-bit Universally Unique Identifier (UUID).
 *
 * @note Thread safety: Thread-safe (immutable value type).
 */
class Uuid {
public:
    constexpr Uuid() noexcept = default;

    explicit constexpr Uuid(const std::array<std::uint8_t, 16>& bytes) noexcept
        : bytes_(bytes) {}

    /**
     * @brief Returns a nil (all-zeros) UUID.
     */
    [[nodiscard]] static constexpr Uuid nil() noexcept {
        return Uuid(std::array<std::uint8_t, 16>{});
    }

    /**
     * @brief Checks if this UUID is all zeros.
     */
    [[nodiscard]] constexpr bool is_nil() const noexcept {
        for (std::size_t i = 0; i < 16; ++i) {
            if (bytes_[i] != 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Gets the underlying 16 bytes of this UUID.
     */
    [[nodiscard]] constexpr const std::array<std::uint8_t, 16>& bytes() const noexcept {
        return bytes_;
    }

    /**
     * @brief Formats this UUID as canonical lowercase 8-4-4-4-12 hex string.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * @brief Parses a 36-character canonical hex UUID string (case-insensitive).
     */
    [[nodiscard]] static Result<Uuid> parse(std::string_view str);

    [[nodiscard]] constexpr auto operator<=>(const Uuid&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const Uuid&) const noexcept = default;

private:
    std::array<std::uint8_t, 16> bytes_{};
};

/**
 * @brief Thread-safe generator producing random RFC-4122 version-4 UUIDs.
 *
 * Callers own and pass generator instances; no global singleton exists.
 *
 * @note Thread safety: Thread-safe (internal mutex synchronization).
 */
class UuidGenerator {
public:
    /**
     * @brief Constructs a generator seeded with std::random_device.
     */
    UuidGenerator();

    /**
     * @brief Constructs a generator seeded deterministically with a fixed 64-bit seed.
     */
    explicit UuidGenerator(std::uint64_t seed);

    UuidGenerator(const UuidGenerator&) = delete;
    UuidGenerator& operator=(const UuidGenerator&) = delete;
    UuidGenerator(UuidGenerator&&) = delete;
    UuidGenerator& operator=(UuidGenerator&&) = delete;

    /**
     * @brief Produces a newly generated version-4 UUID.
     */
    [[nodiscard]] Uuid generate();

    /**
     * @brief Function call operator alias for generate().
     */
    [[nodiscard]] Uuid operator()() {
        return generate();
    }

private:
    std::mutex mutex_;
    std::mt19937_64 engine_;
};

}  // namespace nxtcut::core

namespace std {

template <>
struct hash<nxtcut::core::Uuid> {
    std::size_t operator()(const nxtcut::core::Uuid& id) const noexcept {
        const auto& b = id.bytes();
        std::size_t h = 14695981039346656037ULL;
        for (std::uint8_t byte : b) {
            h ^= static_cast<std::size_t>(byte);
            h *= 1099511628211ULL;
        }
        return h;
    }
};

}  // namespace std
