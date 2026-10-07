#include <freecut/core/version.hpp>

#include <fmt/format.h>

namespace freecut::core {

namespace {
constexpr std::uint32_t kEngineVersionMajor = 0;
constexpr std::uint32_t kEngineVersionMinor = 1;
constexpr std::uint32_t kEngineVersionPatch = 0;
}  // namespace

std::string Version::to_string() const {
    return fmt::format("{}.{}.{}", major, minor, patch);
}

std::string to_string(const Version& ver) {
    return ver.to_string();
}

Version version() noexcept {
    return Version{
        .major = kEngineVersionMajor,
        .minor = kEngineVersionMinor,
        .patch = kEngineVersionPatch,
    };
}

}  // namespace freecut::core
