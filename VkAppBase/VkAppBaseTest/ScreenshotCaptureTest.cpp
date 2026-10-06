#include <gtest/gtest.h>

#include "../ScreenshotCapture.h"

#include <filesystem>
#include <vector>

using namespace VKG;

namespace {

ScreenshotArgs parse(std::vector<const char*> args)
{
    args.insert(args.begin(), "app.exe");
    return parseScreenshotArgs(static_cast<int>(args.size()), const_cast<char**>(args.data()));
}

} // namespace

TEST(ScreenshotCaptureTest, NoArgsMeansNoScreenshot)
{
    const auto a = parse({});
    EXPECT_TRUE(a.path.empty());
    EXPECT_FALSE(a.frame.has_value());
}

TEST(ScreenshotCaptureTest, SpaceAndEqualsForms)
{
    auto a = parse({"--screenshot", "out/a.png", "--screenshot-frame", "12"});
    EXPECT_EQ("out/a.png", a.path);
    EXPECT_EQ(12, a.frame);

    a = parse({"--screenshot=b.png", "--screenshot-frame=3"});
    EXPECT_EQ("b.png", a.path);
    EXPECT_EQ(3, a.frame);
}

TEST(ScreenshotCaptureTest, FrameMayPrecedePathAndLastWins)
{
    auto a = parse({"--screenshot-frame=7", "--screenshot=x.png"});
    EXPECT_EQ("x.png", a.path);
    EXPECT_EQ(7, a.frame);

    a = parse({"--screenshot=1.png", "--screenshot=2.png", "--screenshot-frame", "1", "--screenshot-frame", "9"});
    EXPECT_EQ("2.png", a.path);
    EXPECT_EQ(9, a.frame);
}

TEST(ScreenshotCaptureTest, InvalidFrameIsIgnoredNotFatal)
{
    for (const char* bad : {"abc", "-1", "", "5x", "99999999999"}) {
        const auto a = parse({"--screenshot=x.png", "--screenshot-frame", bad});
        EXPECT_EQ("x.png", a.path) << bad;
        EXPECT_FALSE(a.frame.has_value()) << bad;
    }
}

TEST(ScreenshotCaptureTest, DanglingOptionAndUnknownArgs)
{
    auto a = parse({"model.glb", "--screenshot"}); // value missing
    EXPECT_TRUE(a.path.empty());
    a = parse({"model.glb", "--other", "--screenshot=s.png"});
    EXPECT_EQ("s.png", a.path);
}

TEST(ScreenshotCaptureTest, BgraFormatsAndChannelSwap)
{
    EXPECT_TRUE(isBgraFormat(VK_FORMAT_B8G8R8A8_SRGB));
    EXPECT_TRUE(isBgraFormat(VK_FORMAT_B8G8R8A8_UNORM));
    EXPECT_TRUE(isBgraFormat(VK_FORMAT_B8G8R8A8_SNORM));
    EXPECT_FALSE(isBgraFormat(VK_FORMAT_R8G8B8A8_UNORM));

    uint8_t px[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    swapRedBlue(px, 2);
    const uint8_t expect[8] = {3, 2, 1, 4, 7, 6, 5, 8};
    for (int i = 0; i < 8; ++i) EXPECT_EQ(expect[i], px[i]) << i;

    swapRedBlue(px, 0); // zero pixels: untouched
    EXPECT_EQ(3, px[0]);
}

TEST(ScreenshotCaptureTest, WritePngCreatesDirectoriesAndFile)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "cglib_screenshot_test";
    fs::remove_all(dir);
    const fs::path file = dir / "nested" / "img.png";

    const std::vector<uint8_t> rgba(4 * 2 * 2, 128);
    EXPECT_TRUE(writePng(file.string(), rgba.data(), 2, 2));
    EXPECT_TRUE(fs::exists(file));
    EXPECT_GT(fs::file_size(file), 0u);

    fs::remove_all(dir);
}
