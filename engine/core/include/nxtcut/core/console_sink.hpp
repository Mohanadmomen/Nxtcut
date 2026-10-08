#pragma once

#include <nxtcut/core/log_sink.hpp>

#include <mutex>
#include <ostream>

namespace nxtcut::core {

/**
 * @brief Log sink that writes plain formatted lines to a standard output stream.
 *
 * Does not own the target std::ostream; the stream instance must outlive the sink.
 * Lines are written without terminal color codes and followed by a newline.
 *
 * @note Thread safety: Thread-safe (internal mutex synchronization).
 */
class ConsoleSink final : public LogSink {
public:
    /**
     * @brief Constructs a ConsoleSink bound to the specified output stream.
     *
     * @param out Stream to write formatted log records to. Must outlive this sink.
     */
    explicit ConsoleSink(std::ostream& out);

    ~ConsoleSink() override = default;

    void write(const LogRecord& record) override;
    void flush() override;

private:
    std::ostream& out_;
    mutable std::mutex mutex_;
};

}  // namespace nxtcut::core
