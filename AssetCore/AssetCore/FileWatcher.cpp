#include "FileWatcher.h"

namespace Phantom::Asset {

FileWatcher::Reading FileWatcher::readFile(const std::string& path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path p{ std::u8string(path.begin(), path.end()) };

    Reading r;
    r.exists = fs::exists(p, ec) && !ec && fs::is_regular_file(p, ec) && !ec;
    if (!r.exists) return r;

    r.size = fs::file_size(p, ec);
    if (ec) { r.exists = false; r.size = 0; return r; }
    r.mtime = fs::last_write_time(p, ec);
    if (ec) { r.exists = false; r.size = 0; r.mtime = {}; }
    return r;
}

void FileWatcher::watch(const std::string& path)
{
    Entry e;
    e.confirmed = readFile(path);
    e.hasLastPoll = false;
    entries_[path] = e;
}

void FileWatcher::unwatch(const std::string& path)
{
    entries_.erase(path);
}

void FileWatcher::clear()
{
    entries_.clear();
}

std::vector<std::string> FileWatcher::poll()
{
    std::vector<std::string> changed;
    for (auto& [path, entry] : entries_) {
        const Reading cur = readFile(path);
        if (!entry.hasLastPoll) {
            entry.lastPoll = cur;
            entry.hasLastPoll = true;
            continue;
        }
        if (cur == entry.lastPoll && cur != entry.confirmed) {
            entry.confirmed = cur;
            changed.push_back(path);
        }
        entry.lastPoll = cur;
    }
    return changed;
}

} // namespace Phantom::Asset
