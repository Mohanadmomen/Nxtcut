#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/core/mul_div_detail.hpp>

#include <cstdint>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#include <immintrin.h>
#include <intrin.h>
#endif

namespace nxtcut::core {

namespace {

struct UnsignedDivResult {
    std::uint64_t quotient{0};
    std::uint64_t remainder{0};
};

[[nodiscard]] constexpr std::uint64_t uabs(std::int64_t v) noexcept {
    return (v < 0) ? (0ULL - static_cast<std::uint64_t>(v)) : static_cast<std::uint64_t>(v);
}

struct Uint128 {
    std::uint64_t hi{0};
    std::uint64_t lo{0};
};

[[nodiscard]] constexpr Uint128 mul64(std::uint64_t u, std::uint64_t v) noexcept {
    const std::uint64_t u0 = u & 0xFFFFFFFFULL;
    const std::uint64_t u1 = u >> 32;
    const std::uint64_t v0 = v & 0xFFFFFFFFULL;
    const std::uint64_t v1 = v >> 32;

    const std::uint64_t w0 = u0 * v0;
    const std::uint64_t t = u1 * v0 + (w0 >> 32);
    const std::uint64_t w1 = t & 0xFFFFFFFFULL;
    const std::uint64_t w2 = t >> 32;
    const std::uint64_t w1_prime = u0 * v1 + w1;

    const std::uint64_t lo = (w1_prime << 32) | (w0 & 0xFFFFFFFFULL);
    const std::uint64_t hi = u1 * v1 + w2 + (w1_prime >> 32);
    return {hi, lo};
}

[[nodiscard]] Result<UnsignedDivResult> div128_portable(std::uint64_t ua, std::uint64_t ub,
                                                        std::uint64_t qc) noexcept {
    const Uint128 prod = mul64(ua, ub);
    if (prod.hi >= qc) {
        return make_error(ErrorCode::Overflow,
                          "mul_div intermediate quotient exceeds 64-bit unsigned capacity");
    }
    std::uint64_t q_trunc = 0;
    std::uint64_t rem = 0;
    if (prod.hi == 0) {
        q_trunc = prod.lo / qc;
        rem = prod.lo % qc;
    } else {
        rem = prod.hi;
        for (int i = 63; i >= 0; --i) {
            const std::uint64_t next_bit = (prod.lo >> static_cast<unsigned>(i)) & 1ULL;
            const bool overflow_or_ge =
                (rem >= 0x8000000000000000ULL) || (((rem << 1) | next_bit) >= qc);
            if (overflow_or_ge) {
                q_trunc |= (1ULL << static_cast<unsigned>(i));
                rem = (((rem << 1) | next_bit) - qc);
            } else {
                rem = (rem << 1) | next_bit;
            }
        }
    }
    if (q_trunc > 0x8000000000000000ULL) {
        return make_error(ErrorCode::Overflow,
                          "mul_div intermediate quotient exceeds 64-bit signed magnitude");
    }
    return UnsignedDivResult{q_trunc, rem};
}

#if defined(__SIZEOF_INT128__)
// __extension__ keeps GCC quiet under -Wpedantic (ISO C++ has no 128-bit integer).
__extension__ typedef unsigned __int128 Uint128Native;

[[nodiscard]] Result<UnsignedDivResult> div128_fast(std::uint64_t ua, std::uint64_t ub,
                                                    std::uint64_t qc) noexcept {
    const Uint128Native p = static_cast<Uint128Native>(ua) * static_cast<Uint128Native>(ub);
    const Uint128Native q_128 = p / qc;
    if (q_128 > 0x8000000000000000ULL) {
        return make_error(ErrorCode::Overflow,
                          "mul_div intermediate quotient exceeds 64-bit signed magnitude");
    }
    return UnsignedDivResult{static_cast<std::uint64_t>(q_128), static_cast<std::uint64_t>(p % qc)};
}
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
[[nodiscard]] Result<UnsignedDivResult> div128_fast(std::uint64_t ua, std::uint64_t ub,
                                                    std::uint64_t qc) noexcept {
    unsigned __int64 hi = 0;
    const unsigned __int64 lo = _umul128(ua, ub, &hi);
    if (hi >= qc) {
        return make_error(ErrorCode::Overflow,
                          "mul_div intermediate quotient exceeds 64-bit unsigned capacity");
    }
    unsigned __int64 rem_val = 0;
    const unsigned __int64 q_val = _udiv128(hi, lo, qc, &rem_val);
    if (q_val > 0x8000000000000000ULL) {
        return make_error(ErrorCode::Overflow,
                          "mul_div intermediate quotient exceeds 64-bit signed magnitude");
    }
    return UnsignedDivResult{q_val, rem_val};
}
#endif

[[nodiscard]] Result<std::int64_t> apply_rounding_and_sign(std::uint64_t q_trunc, std::uint64_t rem,
                                                           std::uint64_t qc, bool is_negative,
                                                           RoundingMode mode) noexcept {
    bool add_one = false;
    if (rem != 0) {
        if (mode == RoundingMode::Nearest) {
            add_one = (rem >= (qc - rem));
        } else if (mode == RoundingMode::Floor) {
            add_one = is_negative;
        } else if (mode == RoundingMode::Ceil) {
            add_one = !is_negative;
        }
    }

    if (add_one) {
        if (q_trunc == 0xFFFFFFFFFFFFFFFFULL) {
            return make_error(ErrorCode::Overflow, "mul_div rounding overflow");
        }
        ++q_trunc;
    }

    if (!is_negative) {
        if (q_trunc > 0x7FFFFFFFFFFFFFFFULL) {
            return make_error(ErrorCode::Overflow, "mul_div positive result exceeds int64 maximum");
        }
        return static_cast<std::int64_t>(q_trunc);
    }

    if (q_trunc > 0x8000000000000000ULL) {
        return make_error(ErrorCode::Overflow,
                          "mul_div negative result exceeds int64 minimum magnitude");
    }
    if (q_trunc == 0x8000000000000000ULL) {
        return INT64_MIN;
    }
    return -static_cast<std::int64_t>(q_trunc);
}

}  // namespace

namespace detail {

Result<std::int64_t> mul_div_portable(std::int64_t a, std::int64_t b, std::int64_t c,
                                      RoundingMode mode) noexcept {
    if (c == 0) {
        return make_error(ErrorCode::InvalidArgument, "Division by zero in mul_div");
    }

    if (a == 0 || b == 0) {
        return static_cast<std::int64_t>(0);
    }

    const bool is_negative = (a < 0) ^ (b < 0) ^ (c < 0);
    const std::uint64_t ua = uabs(a);
    const std::uint64_t ub = uabs(b);
    const std::uint64_t qc = uabs(c);

    const auto div_res = div128_portable(ua, ub, qc);
    if (!div_res) {
        return tl::unexpected(div_res.error());
    }

    return apply_rounding_and_sign(div_res->quotient, div_res->remainder, qc, is_negative, mode);
}

}  // namespace detail

Result<std::int64_t> mul_div(std::int64_t a, std::int64_t b, std::int64_t c,
                             RoundingMode mode) noexcept {
#if defined(__SIZEOF_INT128__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64)))
    if (c == 0) {
        return make_error(ErrorCode::InvalidArgument, "Division by zero in mul_div");
    }

    if (a == 0 || b == 0) {
        return static_cast<std::int64_t>(0);
    }

    const bool is_negative = (a < 0) ^ (b < 0) ^ (c < 0);
    const std::uint64_t ua = uabs(a);
    const std::uint64_t ub = uabs(b);
    const std::uint64_t qc = uabs(c);

    const auto div_res = div128_fast(ua, ub, qc);
    if (!div_res) {
        return tl::unexpected(div_res.error());
    }

    return apply_rounding_and_sign(div_res->quotient, div_res->remainder, qc, is_negative, mode);
#else
    return detail::mul_div_portable(a, b, c, mode);
#endif
}

}  // namespace nxtcut::core