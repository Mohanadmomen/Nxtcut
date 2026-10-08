#pragma once

#include <concepts>
#include <type_traits>

namespace nxtcut::model::test {

template <class From, class To>
concept CanConvert = std::is_convertible_v<From, To>;

template <class To, class From>
concept CanConstruct = std::is_constructible_v<To, From>;

template <class L, class R>
concept CanAdd = requires(L l, R r) { l + r; };

template <class L, class R>
concept CanSubtract = requires(L l, R r) { l - r; };

template <class L, class R>
concept CanEqual = requires(L l, R r) { l == r; };

template <class L, class R>
concept CanOrder = requires(L l, R r) { l <=> r; };

}  // namespace nxtcut::model::test
