#include <nxtcut/core/memory_sink.hpp>

#include <algorithm>

namespace nxtcut::core {

MemorySink::MemorySink(std::size_t max_entries)
    : max_entries_(std::max<std::size_t>(max_entries, 1)) {}

void MemorySink::write(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(Entry{
        record.level,
        record.unix_millis,
        std::string(record.logger_name),
        std::string(record.message),
    });
    while (entries_.size() > max_entries_) {
        entries_.pop_front();
    }
}

void MemorySink::flush() {}

std::vector<MemorySink::Entry> MemorySink::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<Entry>(entries_.begin(), entries_.end());
}

void MemorySink::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

std::size_t MemorySink::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

}  // namespace nxtcut::core
