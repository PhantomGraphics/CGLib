#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::Asset {

// Poll-based file change detector (Blender->Universe authoring loop plan, Phase 2 item 1/6/7 --
// "file watcher, debounce" / "最初はfile watcherによるreload"). There is no OS
// file-system-event API here (inotify/ReadDirectoryChangesW etc.) -- a consumer calls poll()
// periodically instead (Universe's hot-reload wiring does this on a wall-clock timer, not every
// frame, to keep the stat() cost negligible), and gets back the set of watched paths whose
// content has actually settled into a new state.
//
// Debounce: a change is only reported once the SAME (exists, size, mtime) reading has been
// observed on two consecutive poll() calls. A file mid-write (an exporter still appending to it)
// keeps changing size/mtime poll to poll and is therefore never reported until it stops -- this
// is the same "settle before acting" shape as the plan's hot-reload deliverable calls for,
// applied to change *detection* rather than the GPU-side swap itself (see
// CGApp/Universe/Rendering/GltfRenderer.cpp's hot-reload wiring for that part; this class knows
// nothing about GPU resources or Universe).
class FileWatcher {
public:
    // (Re-)starts watching `path`. Safe to call again for an already-watched path -- resets its
    // baseline to the file's current on-disk state (or "missing", if it doesn't exist yet), so
    // the next poll() never itself reports a change for a path just (re-)registered.
    void watch(const std::string& path);

    void unwatch(const std::string& path);
    void clear();

    // Reads every watched path's current state and returns those whose settled state (see class
    // comment) differs from the last-reported one. Cheap (one filesystem stat per watched path)
    // but still real I/O -- callers should not call this every frame.
    std::vector<std::string> poll();

private:
    struct Reading {
        bool exists = false;
        uintmax_t size = 0;
        std::filesystem::file_time_type mtime{};

        bool operator==(const Reading& o) const {
            return exists == o.exists && size == o.size && mtime == o.mtime;
        }
        bool operator!=(const Reading& o) const { return !(*this == o); }
    };

    struct Entry {
        Reading confirmed;      // last reading actually reported (or the baseline from watch())
        Reading lastPoll;       // raw reading from the previous poll() call
        bool hasLastPoll = false;
    };

    static Reading readFile(const std::string& path);

    std::unordered_map<std::string, Entry> entries_;
};

} // namespace Phantom::Asset
