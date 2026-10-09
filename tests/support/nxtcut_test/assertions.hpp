#pragma once

#include <nxtcut/core/error.hpp>
#include <nxtcut/core/result.hpp>

#include <gtest/gtest.h>

namespace nxtcut::test {

template <class R>
[[nodiscard]] ::testing::AssertionResult is_ok(const R& result) {
    if (result.has_value()) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << result.error().to_string();
}

template <class R>
[[nodiscard]] ::testing::AssertionResult is_error(const R& result, core::ErrorCode expected_code) {
    if (result.has_value()) {
        return ::testing::AssertionFailure()
               << "expected error " << core::to_string(expected_code) << ", got ok";
    }
    if (result.error().code() != expected_code) {
        return ::testing::AssertionFailure() << "expected error " << core::to_string(expected_code)
                                             << ", got " << result.error().to_string();
    }
    return ::testing::AssertionSuccess();
}

}  // namespace nxtcut::test

namespace nxtcut::commands::test {

using nxtcut::test::is_error;
using nxtcut::test::is_ok;

}  // namespace nxtcut::commands::test
