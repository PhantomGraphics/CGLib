#include "pch.h"
#include "gtest/gtest.h"

#include "../Graphics/FastPng.h"
#include "../ThirdParty/stb/stb_image.h"  // declarations only: the implementation lives in ImageFileReader.cpp

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

using namespace Phantom::Graphics;

namespace {

// Decodes with stb_image (an independent implementation) and returns the raw interleaved bytes.
bool decode(const std::vector<uint8_t>& png, int channels, int& w, int& h, std::vector<uint8_t>& out)
{
	int n = 0;
	unsigned char* data = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &w, &h, &n, channels);
	if (!data) return false;
	out.assign(data, data + static_cast<size_t>(w) * h * channels);
	stbi_image_free(data);
	return true;
}

std::vector<uint8_t> noisy(uint32_t w, uint32_t h, int channels, uint32_t seed)
{
	std::vector<uint8_t> v(static_cast<size_t>(w) * h * channels);
	uint32_t s = seed;
	for (auto& b : v) { s = s * 1664525u + 1013904223u; b = static_cast<uint8_t>(s >> 24); }
	return v;
}

} // namespace

TEST(FastPngTest, RoundTripsEveryChannelCount)
{
	for (int channels = 1; channels <= 4; ++channels) {
		const auto pixels = noisy(37, 23, channels, 7u + channels);
		const auto png = encodePngFast(pixels.data(), 37, 23, channels);
		ASSERT_FALSE(png.empty());
		int w = 0, h = 0;
		std::vector<uint8_t> back;
		ASSERT_TRUE(decode(png, channels, w, h, back)) << "channels=" << channels;
		EXPECT_EQ(37, w);
		EXPECT_EQ(23, h);
		EXPECT_EQ(pixels, back) << "channels=" << channels;
	}
}

TEST(FastPngTest, FlatImagesCompressAndLongRunsSpanManyMatches)
{
	// 600x300 RGBA, one colour with a few distinct pixels: far longer than a single 258-byte match and the 32 KiB window.
	std::vector<uint8_t> pixels(600u * 300u * 4u);
	for (size_t i = 0; i < pixels.size(); i += 4) { pixels[i] = 20; pixels[i + 1] = 30; pixels[i + 2] = 40; pixels[i + 3] = 255; }
	pixels[4 * (150 * 600 + 300) + 0] = 255;
	const auto png = encodePngFast(pixels.data(), 600, 300, 4);
	EXPECT_LT(png.size(), pixels.size() / 50);
	int w = 0, h = 0;
	std::vector<uint8_t> back;
	ASSERT_TRUE(decode(png, 4, w, h, back));
	EXPECT_EQ(pixels, back);
}

TEST(FastPngTest, RespectsStrideAndSwapsBlueAndRed)
{
	// 5x4 BGRA image inside a wider buffer (stride 32 bytes instead of 20).
	const uint32_t w = 5, h = 4;
	const size_t stride = 32;
	std::vector<uint8_t> padded(stride * h, 0xee);
	std::vector<uint8_t> expectedRgba(w * h * 4);
	for (uint32_t y = 0; y < h; ++y) {
		for (uint32_t x = 0; x < w; ++x) {
			const uint8_t b = static_cast<uint8_t>(10 * x + y), g = static_cast<uint8_t>(50 + x), r = static_cast<uint8_t>(200 - y), a = 255;
			uint8_t* p = &padded[y * stride + x * 4];
			p[0] = b; p[1] = g; p[2] = r; p[3] = a;
			uint8_t* q = &expectedRgba[(y * w + x) * 4];
			q[0] = r; q[1] = g; q[2] = b; q[3] = a;
		}
	}
	const auto png = encodePngFast(padded.data(), w, h, 4, stride, /*swapRedBlue=*/true);
	int iw = 0, ih = 0;
	std::vector<uint8_t> back;
	ASSERT_TRUE(decode(png, 4, iw, ih, back));
	EXPECT_EQ(expectedRgba, back);
}

TEST(FastPngTest, RejectsInvalidArguments)
{
	const std::vector<uint8_t> px(16, 0);
	EXPECT_TRUE(encodePngFast(nullptr, 2, 2, 4).empty());
	EXPECT_TRUE(encodePngFast(px.data(), 0, 2, 4).empty());
	EXPECT_TRUE(encodePngFast(px.data(), 2, 0, 4).empty());
	EXPECT_TRUE(encodePngFast(px.data(), 2, 2, 5).empty());
	EXPECT_TRUE(encodePngFast(px.data(), 2, 2, 4, 4).empty());  // stride smaller than a row
}

TEST(FastPngTest, WritesAFileThatTheStbReaderLoads)
{
	const auto pixels = noisy(16, 9, 4, 3u);
	const auto path = (std::filesystem::temp_directory_path() / "fastpng_test.png").string();
	ASSERT_TRUE(writePngFast(path, pixels.data(), 16, 9, 4));
	int w = 0, h = 0, n = 0;
	unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
	ASSERT_NE(nullptr, data);
	EXPECT_EQ(16, w);
	EXPECT_EQ(9, h);
	EXPECT_EQ(pixels, std::vector<uint8_t>(data, data + pixels.size()));
	stbi_image_free(data);
	std::filesystem::remove(path);
	EXPECT_FALSE(writePngFast((std::filesystem::temp_directory_path() / "no_such_dir_xyz" / "a.png").string(), pixels.data(), 16, 9, 4));
}

TEST(FastPngTest, IsFastOnAScreenshotSizedImage)
{
	// 1466x684 like the viewport captures, mostly flat with some structure; the point is that this stays well under a
	// second even in a Debug build (stb needed ~4 s).
	const uint32_t w = 1466, h = 684;
	std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4, 0);
	for (uint32_t y = 0; y < h; ++y)
		for (uint32_t x = 0; x < w; ++x) {
			uint8_t* p = &pixels[(static_cast<size_t>(y) * w + x) * 4];
			const bool line = (x % 97 == 0) || (y % 61 == 0);
			p[0] = line ? 200 : 40; p[1] = line ? 220 : 44; p[2] = line ? 250 : 52; p[3] = 255;
		}
	const auto t0 = std::chrono::steady_clock::now();
	const auto png = encodePngFast(pixels.data(), w, h, 4);
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	EXPECT_LT(seconds, 1.5);
	int iw = 0, ih = 0;
	std::vector<uint8_t> back;
	ASSERT_TRUE(decode(png, 4, iw, ih, back));
	EXPECT_EQ(pixels, back);
	std::printf("[FastPng] %ux%u -> %zu bytes in %.3f s\n", w, h, png.size(), seconds);
}
