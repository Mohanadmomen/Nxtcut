#include <nxtcut/core/uuid.hpp>

#include <fmt/format.h>

namespace nxtcut::core {

namespace {

[[nodiscard]] constexpr int parse_hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] bool parse_byte(char hi, char lo, std::uint8_t& out) noexcept {
    const int d_hi = parse_hex_digit(hi);
    const int d_lo = parse_hex_digit(lo);
    if (d_hi < 0 || d_lo < 0) {
        return false;
    }
    out = static_cast<std::uint8_t>((d_hi << 4) | d_lo);
    return true;
}

}  // namespace

std::string Uuid::to_string() const {
    return fmt::format(
        "{:02x}{:02x}{:02x}{:02x}-"
        "{:02x}{:02x}-"
        "{:02x}{:02x}-"
        "{:02x}{:02x}-"
        "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        bytes_[0], bytes_[1], bytes_[2], bytes_[3], bytes_[4], bytes_[5], bytes_[6], bytes_[7],
        bytes_[8], bytes_[9], bytes_[10], bytes_[11], bytes_[12], bytes_[13], bytes_[14],
        bytes_[15]);
}

Result<Uuid> Uuid::parse(std::string_view str) {
    if (str.size() != 36) {
        return make_error(ErrorCode::InvalidArgument, "UUID string must be exactly 36 characters");
    }
    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
        return make_error(ErrorCode::InvalidArgument, "Invalid UUID hyphen format");
    }

    std::array<std::uint8_t, 16> bytes{};
    if (!parse_byte(str[0], str[1], bytes[0]) || !parse_byte(str[2], str[3], bytes[1]) ||
        !parse_byte(str[4], str[5], bytes[2]) || !parse_byte(str[6], str[7], bytes[3])) {
        return make_error(ErrorCode::InvalidArgument, "Invalid hex digit in UUID segment 1");
    }

    if (!parse_byte(str[9], str[10], bytes[4]) || !parse_byte(str[11], str[12], bytes[5])) {
        return make_error(ErrorCode::InvalidArgument, "Invalid hex digit in UUID segment 2");
    }

    if (!parse_byte(str[14], str[15], bytes[6]) || !parse_byte(str[16], str[17], bytes[7])) {
        return make_error(ErrorCode::InvalidArgument, "Invalid hex digit in UUID segment 3");
    }

    if (!parse_byte(str[19], str[20], bytes[8]) || !parse_byte(str[21], str[22], bytes[9])) {
        return make_error(ErrorCode::InvalidArgument, "Invalid hex digit in UUID segment 4");
    }

    if (!parse_byte(str[24], str[25], bytes[10]) || !parse_byte(str[26], str[27], bytes[11]) ||
        !parse_byte(str[28], str[29], bytes[12]) || !parse_byte(str[30], str[31], bytes[13]) ||
        !parse_byte(str[32], str[33], bytes[14]) || !parse_byte(str[34], str[35], bytes[15])) {
        return make_error(ErrorCode::InvalidArgument, "Invalid hex digit in UUID segment 5");
    }

    return Uuid(bytes);
}

UuidGenerator::UuidGenerator() {
    std::random_device rd;
    const std::uint64_t seed =
        (static_cast<std::uint64_t>(rd()) << 32) | static_cast<std::uint64_t>(rd());
    engine_.seed(seed);
}

UuidGenerator::UuidGenerator(std::uint64_t seed) : engine_(seed) {}

Uuid UuidGenerator::generate() {
    std::array<std::uint8_t, 16> bytes{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::uint64_t w1 = engine_();
        const std::uint64_t w2 = engine_();
        for (std::size_t i = 0; i < 8; ++i) {
            bytes[i] = static_cast<std::uint8_t>((w1 >> (i * 8)) & 0xFF);
            bytes[i + 8] = static_cast<std::uint8_t>((w2 >> (i * 8)) & 0xFF);
        }
    }

    // Version 4: bits 4-7 of byte 6 set to 0100 (0x40)
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x40);
    // Variant 1: bits 6-7 of byte 8 set to 10 (0x80)
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80);

    return Uuid(bytes);
}

}  // namespace nxtcut::core
