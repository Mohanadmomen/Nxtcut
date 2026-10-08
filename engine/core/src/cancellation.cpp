#include <nxtcut/core/cancellation.hpp>

namespace nxtcut::core {

bool CancellationToken::is_cancelled() const noexcept {
    return state_ != nullptr && state_->load(std::memory_order_acquire);
}

Status CancellationToken::check() const {
    if (is_cancelled()) {
        return make_error(ErrorCode::Cancelled, "operation cancelled");
    }
    return Status{};
}

CancellationSource::CancellationSource() : state_(std::make_shared<std::atomic<bool>>(false)) {}

void CancellationSource::request_cancel() noexcept {
    if (state_) {
        state_->store(true, std::memory_order_release);
    }
}

bool CancellationSource::is_cancelled() const noexcept {
    return state_ != nullptr && state_->load(std::memory_order_acquire);
}

CancellationToken CancellationSource::token() const {
    return CancellationToken(state_);
}

}  // namespace nxtcut::core
