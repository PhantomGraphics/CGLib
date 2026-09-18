#include "gtest/gtest.h"

#include "../AssetCore/FileWatcher.h"

#include <filesystem>
#include <fstream>
#include <random>

using namespace Phantom::Asset;

namespace {

std::filesystem::path tempDir()
{
    static std::mt19937 rng{ std::random_device{}() };
    const auto dir = std::filesystem::temp_directory_path()
        / ("FileWatcherTest_" + std::to_string(rng()));
    std::filesystem::create_directories(dir);
    return dir;
}

void writeFile(const std::filesystem::path& p, const std::string& content)
{
    std::ofstream(p, std::ios::binary | std::ios::trunc) << content;
}

} // namespace

TEST(FileWatcher, NoChangeReportedWhenFileUntouched)
{
    const auto dir = tempDir();
    const auto file = dir / "a.txt";
    writeFile(file, "hello");

    FileWatcher w;
    w.watch(file.string());
    EXPECT_TRUE(w.poll().empty());
    EXPECT_TRUE(w.poll().empty());

    std::filesystem::remove_all(dir);
}

TEST(FileWatcher, ContentChangeReportedAfterSettling)
{
    const auto dir = tempDir();
    const auto file = dir / "a.txt";
    writeFile(file, "hello");

    FileWatcher w;
    w.watch(file.string());
    ASSERT_TRUE(w.poll().empty());

    writeFile(file, "hello world, a longer body");
    // First poll after the write only records the new raw reading -- not reported yet (a real
    // exporter could still be mid-write at this instant).
    EXPECT_TRUE(w.poll().empty());
    // Second poll sees the same (now-settled) reading as the first -> reported.
    const auto changed = w.poll();
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], file.string());
    // Further polls with no further writes report nothing new.
    EXPECT_TRUE(w.poll().empty());

    std::filesystem::remove_all(dir);
}

TEST(FileWatcher, MidWriteChurnIsNotReportedUntilItStops)
{
    const auto dir = tempDir();
    const auto file = dir / "a.txt";
    writeFile(file, "1");

    FileWatcher w;
    w.watch(file.string());
    ASSERT_TRUE(w.poll().empty());

    // Simulate an exporter still appending -- size changes between every poll, so the debounce
    // never sees two consecutive identical readings and must not report a change.
    writeFile(file, "12");
    EXPECT_TRUE(w.poll().empty());
    writeFile(file, "123");
    EXPECT_TRUE(w.poll().empty());
    writeFile(file, "1234");
    EXPECT_TRUE(w.poll().empty());

    // Now it stops changing -- the next poll sees the same reading twice and reports it.
    const auto changed = w.poll();
    EXPECT_EQ(changed.size(), 1u);

    std::filesystem::remove_all(dir);
}

TEST(FileWatcher, CreationAfterWatchingAMissingPathIsReported)
{
    const auto dir = tempDir();
    const auto file = dir / "not_yet.txt";

    FileWatcher w;
    w.watch(file.string()); // doesn't exist yet
    EXPECT_TRUE(w.poll().empty());

    writeFile(file, "now it exists");
    EXPECT_TRUE(w.poll().empty()); // raw reading recorded, not yet settled
    const auto changed = w.poll();
    EXPECT_EQ(changed.size(), 1u);

    std::filesystem::remove_all(dir);
}

TEST(FileWatcher, UnwatchStopsReporting)
{
    const auto dir = tempDir();
    const auto file = dir / "a.txt";
    writeFile(file, "hello");

    FileWatcher w;
    w.watch(file.string());
    ASSERT_TRUE(w.poll().empty());

    w.unwatch(file.string());
    writeFile(file, "changed");
    EXPECT_TRUE(w.poll().empty());
    EXPECT_TRUE(w.poll().empty());

    std::filesystem::remove_all(dir);
}

TEST(FileWatcher, OnlyTheChangedPathIsReportedAmongMultiple)
{
    const auto dir = tempDir();
    const auto fileA = dir / "a.txt";
    const auto fileB = dir / "b.txt";
    writeFile(fileA, "hello");
    writeFile(fileB, "world");

    FileWatcher w;
    w.watch(fileA.string());
    w.watch(fileB.string());
    ASSERT_TRUE(w.poll().empty());

    writeFile(fileB, "a much longer body than before");
    w.poll();
    const auto changed = w.poll();
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], fileB.string());

    std::filesystem::remove_all(dir);
}

TEST(FileWatcher, ReWatchResetsBaselineWithoutReportingImmediately)
{
    const auto dir = tempDir();
    const auto file = dir / "a.txt";
    writeFile(file, "hello");

    FileWatcher w;
    w.watch(file.string());
    ASSERT_TRUE(w.poll().empty());

    writeFile(file, "changed content here");
    w.poll();
    ASSERT_EQ(w.poll().size(), 1u);

    // Re-watching adopts the CURRENT on-disk state as the new baseline (e.g. Universe re-enabling
    // hot reload for an asset it just finished loading) -- no further changes should be reported
    // until the file changes again.
    w.watch(file.string());
    EXPECT_TRUE(w.poll().empty());
    EXPECT_TRUE(w.poll().empty());

    std::filesystem::remove_all(dir);
}
