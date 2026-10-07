#pragma once

#include <nxtcut/core/log_sink.hpp>
#include <nxtcut/core/result.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>

namespace nxtcut::core {

/**
 * @brief Log sink that persists formatted log lines to a local filesystem file.
 *
 * Line endings are written explicitly as LF ('\n') in binary mode to ensure cross-platform consistency.
 * Log entries at Warn severity or higher are flushed immediately to disk.
 *
 * @note Thread safety: Thread-safe (internal mutex synchronization).
 */
class FileSink final : public LogSink {
public:
    enum class Mode {
        Append,
        Truncate,
    };

    struct ConstructToken {
    private:
        friend class FileSink;
        explicit ConstructToken() = default;
    };

    FileSink(ConstructToken, std::ofstream stream);
    ~FileSink() override;

    /**
     * @brief Creates a FileSink writing to the given path.
     *
     * @param path Target filesystem path.
     * @param mode Open mode (Append or Truncate).
     * @return Unique pointer to FileSink or ErrorCode::IoError if file cannot be opened.
     */
    [[nodiscard]] static Result<std::unique_ptr<FileSink>> create(const std::filesystem::path& path,
                                                                   Mode mode);

    void write(const LogRecord& record) override;
    void flush() override;

private:
    std::ofstream stream_;
    mutable std::mutex mutex_;
};

}  // namespace nxtcut::core
