#pragma once

#include <nxtcut/core/log_sink.hpp>

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace nxtcut::core {

/**
 * @brief Thread-safe ring buffer log sink holding up to a fixed maximum number of entries in memory.
 *
 * Oldest records are dropped when capacity is exceeded. Designed to feed in-app UI log viewers.
 *
 * @note Thread safety: Thread-safe (internal mutex synchronization).
 */
class MemorySink final : public LogSink {
public:
    /**
     * @brief Owned log record entry stored in memory.
     */
    struct Entry {
        LogLevel level{LogLevel::Info};
        std::int64_t unix_millis{0};
        std::string logger_name;
        std::string message;

        bool operator==(const Entry& other) const = default;
    };

    /**
     * @brief Constructs a MemorySink with a maximum capacity.
     *
     * @param max_entries Maximum entries to retain. If less than 1, clamped to 1.
     */
    explicit MemorySink(std::size_t max_entries);

    ~MemorySink() override = default;

    void write(const LogRecord& record) override;
    void flush() override;

    /**
     * @brief Returns an atomic snapshot copy of all currently buffered entries.
     */
    [[nodiscard]] std::vector<Entry> snapshot() const;

    /**
     * @brief Clears all buffered entries.
     */
    void clear();

    /**
     * @brief Returns the number of currently buffered entries.
     */
    [[nodiscard]] std::size_t size() const;

private:
    std::size_t max_entries_;
    std::deque<Entry> entries_;
    mutable std::mutex mutex_;
};

}  // namespace nxtcut::core
