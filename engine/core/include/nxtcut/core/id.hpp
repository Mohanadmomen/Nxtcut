#pragma once

#include <nxtcut/core/uuid.hpp>

#include <compare>
#include <functional>
#include <string>

namespace nxtcut::core {

/**
 * @brief Strongly-typed identifier wrapping a Uuid.
 *
 * Types tagged with distinct Tag template parameters cannot be implicitly
 * or explicitly converted into each other, preventing identifier misuse at compile-time.
 *
 * @tparam Tag Distinct tag type identifying the domain entity (e.g. struct ClipTag).
 * @note Thread safety: Thread-safe (immutable value type).
 */
template <class Tag>
class Id {
public:
    constexpr Id() noexcept : uuid_(Uuid::nil()) {}

    explicit constexpr Id(Uuid u) noexcept : uuid_(u) {}

    [[nodiscard]] static constexpr Id nil() noexcept {
        return Id(Uuid::nil());
    }

    [[nodiscard]] constexpr const Uuid& uuid() const noexcept {
        return uuid_;
    }

    [[nodiscard]] constexpr bool is_nil() const noexcept {
        return uuid_.is_nil();
    }

    [[nodiscard]] std::string to_string() const {
        return uuid_.to_string();
    }

    [[nodiscard]] constexpr auto operator<=>(const Id&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const Id&) const noexcept = default;

private:
    Uuid uuid_{Uuid::nil()};
};

}  // namespace nxtcut::core

namespace std {

template <class Tag>
struct hash<nxtcut::core::Id<Tag>> {
    std::size_t operator()(const nxtcut::core::Id<Tag>& id) const noexcept {
        return std::hash<nxtcut::core::Uuid>{}(id.uuid());
    }
};

}  // namespace std
