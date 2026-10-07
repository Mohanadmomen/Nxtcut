#include <nxtcut/core/error.hpp>

#include <fmt/format.h>

namespace nxtcut::core {

std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::OutOfRange:
            return "OutOfRange";
        case ErrorCode::Overflow:
            return "Overflow";
        case ErrorCode::NotFound:
            return "NotFound";
        case ErrorCode::AlreadyExists:
            return "AlreadyExists";
        case ErrorCode::IoError:
            return "IoError";
        case ErrorCode::Corrupt:
            return "Corrupt";
        case ErrorCode::Unsupported:
            return "Unsupported";
        case ErrorCode::Cancelled:
            return "Cancelled";
        case ErrorCode::Internal:
            return "Internal";
    }
    return "Unknown";
}

Error::Error(ErrorCode code, std::string message) noexcept
    : code_(code), message_(std::move(message)) {}

Error Error::with_context(std::string_view context) const {
    if (message_.empty()) {
        return Error(code_, std::string(context));
    }
    return Error(code_, fmt::format("{}: {}", context, message_));
}

std::string Error::to_string() const {
    if (message_.empty()) {
        return std::string(nxtcut::core::to_string(code_));
    }
    return fmt::format("{}: {}", nxtcut::core::to_string(code_), message_);
}

}  // namespace nxtcut::core
