#include <nxtcut/core/error.hpp>
#include <nxtcut/core/result.hpp>

#include <gtest/gtest.h>

namespace nxtcut::core {
namespace {

TEST(ErrorTest, ConstructWithCodeAndMessage) {
    const Error err(ErrorCode::NotFound, "resource not found");
    EXPECT_EQ(err.code(), ErrorCode::NotFound);
    EXPECT_EQ(err.message(), "resource not found");
}

TEST(ErrorTest, WithContextChainsContextCorrectly) {
    const Error original(ErrorCode::IoError, "file not accessible");
    const Error chained = original.with_context("timeline_loader");

    EXPECT_EQ(chained.code(), ErrorCode::IoError);
    EXPECT_EQ(chained.message(), "timeline_loader: file not accessible");
}

TEST(ErrorTest, WithContextOnEmptyMessage) {
    const Error original(ErrorCode::Corrupt);
    const Error chained = original.with_context("parser");

    EXPECT_EQ(chained.code(), ErrorCode::Corrupt);
    EXPECT_EQ(chained.message(), "parser");
}

TEST(ErrorTest, ToStringFormatting) {
    const Error err(ErrorCode::InvalidArgument, "value out of bounds");
    EXPECT_EQ(err.to_string(), "InvalidArgument: value out of bounds");

    const Error empty_err(ErrorCode::Internal);
    EXPECT_EQ(empty_err.to_string(), "Internal");
}

TEST(ErrorTest, EqualityAndInequality) {
    const Error e1(ErrorCode::AlreadyExists, "key_1");
    const Error e2(ErrorCode::AlreadyExists, "key_1");
    const Error e3(ErrorCode::AlreadyExists, "key_2");
    const Error e4(ErrorCode::Unsupported, "key_1");

    EXPECT_EQ(e1, e2);
    EXPECT_NE(e1, e3);
    EXPECT_NE(e1, e4);
}

TEST(ErrorTest, ErrorCodeToStringCoverage) {
    EXPECT_EQ(to_string(ErrorCode::InvalidArgument), "InvalidArgument");
    EXPECT_EQ(to_string(ErrorCode::OutOfRange), "OutOfRange");
    EXPECT_EQ(to_string(ErrorCode::Overflow), "Overflow");
    EXPECT_EQ(to_string(ErrorCode::NotFound), "NotFound");
    EXPECT_EQ(to_string(ErrorCode::AlreadyExists), "AlreadyExists");
    EXPECT_EQ(to_string(ErrorCode::IoError), "IoError");
    EXPECT_EQ(to_string(ErrorCode::Corrupt), "Corrupt");
    EXPECT_EQ(to_string(ErrorCode::Unsupported), "Unsupported");
    EXPECT_EQ(to_string(ErrorCode::Cancelled), "Cancelled");
    EXPECT_EQ(to_string(ErrorCode::Internal), "Internal");
}

TEST(ResultTest, ResultSuccessAndFailure) {
    const auto compute_success = []() -> Result<int> { return 123; };
    const auto compute_failure = []() -> Result<int> {
        return make_error(ErrorCode::OutOfRange, "value too large");
    };

    const auto res_ok = compute_success();
    ASSERT_TRUE(res_ok.has_value());
    EXPECT_EQ(*res_ok, 123);

    const auto res_err = compute_failure();
    ASSERT_FALSE(res_err.has_value());
    EXPECT_EQ(res_err.error().code(), ErrorCode::OutOfRange);
    EXPECT_EQ(res_err.error().message(), "value too large");
}

TEST(ResultTest, StatusVoidResult) {
    const auto op_ok = []() -> Status { return {}; };
    const auto op_fail = []() -> Status {
        return make_error(ErrorCode::Cancelled, "user cancelled");
    };

    const auto status_ok = op_ok();
    EXPECT_TRUE(status_ok.has_value());

    const auto status_fail = op_fail();
    ASSERT_FALSE(status_fail.has_value());
    EXPECT_EQ(status_fail.error().code(), ErrorCode::Cancelled);
}

TEST(ResultTest, MakeErrorConstruction) {
    const std::string dynamic_str = "dynamic string";
    const Result<int> res1 = make_error(ErrorCode::IoError, dynamic_str);
    ASSERT_FALSE(res1.has_value());
    EXPECT_EQ(res1.error().code(), ErrorCode::IoError);
    EXPECT_EQ(res1.error().message(), "dynamic string");

    const Result<int> res2 = make_error(ErrorCode::IoError, "string view literal");
    ASSERT_FALSE(res2.has_value());
    EXPECT_EQ(res2.error().code(), ErrorCode::IoError);
    EXPECT_EQ(res2.error().message(), "string view literal");
}

}  // namespace
}  // namespace nxtcut::core
