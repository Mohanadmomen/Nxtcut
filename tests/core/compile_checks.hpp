#pragma once

namespace nxtcut::core::test {

template <class L, class R>
concept CanAdd = requires(L l, R r) { l + r; };

template <class L, class R>
concept CanSubtract = requires(L l, R r) { l - r; };

template <class L, class R>
concept CanEqual = requires(L l, R r) { l == r; };

template <class L, class R>
concept CanOrder = requires(L l, R r) { l <=> r; };

}  // namespace nxtcut::core::test
