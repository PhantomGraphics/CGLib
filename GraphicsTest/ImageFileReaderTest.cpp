#include "pch.h"
#include "gtest/gtest.h"

#include "../../CGLib/Graphics/ImageFileReader.h"
#include "../../CGLib/Graphics/ImageFileWriter.h"
#include "../../CGLib/Graphics/Image.h"

#include <filesystem>
#include <string>

using namespace Phantom::Graphics;

static Imageuc createTestImage(const int w, const int h)
{
	Imageuc img(w, h);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			// 簡単なパターン（識別しやすい色）
			if (x == 0 && y == 0) img.setColor(x, y, ColorRGBAuc(255, 0, 0, 255));   // red
			else if (x == 1 && y == 0) img.setColor(x, y, ColorRGBAuc(0, 255, 0, 255)); // green
			else if (x == 0 && y == 1) img.setColor(x, y, ColorRGBAuc(0, 0, 255, 255)); // blue
			else img.setColor(x, y, ColorRGBAuc(255, 255, 255, 255));                // white
		}
	}
	return img;
}

static void compareImagesEqual(const Imageuc& a, const Imageuc& b)
{
	EXPECT_EQ(a.getWidth(), b.getWidth());
	EXPECT_EQ(a.getHeight(), b.getHeight());
	for (int y = 0; y < a.getHeight(); ++y) {
		for (int x = 0; x < a.getWidth(); ++x) {
			const auto ca = a.getColor(x, y);
			const auto cb = b.getColor(x, y);
			EXPECT_EQ(ca.x, cb.x) << "x,y = " << x << "," << y;
			EXPECT_EQ(ca.y, cb.y) << "x,y = " << x << "," << y;
			EXPECT_EQ(ca.z, cb.z) << "x,y = " << x << "," << y;
			EXPECT_EQ(ca.w, cb.w) << "x,y = " << x << "," << y;
		}
	}
}

TEST(ImageFileReaderTest, ReadWritePNG)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileReaderTest_tmp.png";

	ASSERT_TRUE(writer.write(path, img));
	ASSERT_TRUE(std::filesystem::exists(path));

	ImageFileReader reader;
	ASSERT_TRUE(reader.read(path));
	const auto out = reader.toImage();

	compareImagesEqual(img, out);

	std::filesystem::remove(path);
}

TEST(ImageFileReaderTest, ReadWriteBMP)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileReaderTest_tmp.bmp";

	ASSERT_TRUE(writer.write(path, img));
	ASSERT_TRUE(std::filesystem::exists(path));

	ImageFileReader reader;
	ASSERT_TRUE(reader.read(path));
	const auto out = reader.toImage();

	compareImagesEqual(img, out);

	std::filesystem::remove(path);
}

TEST(ImageFileReaderTest, ReadWriteJPG)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileReaderTest_tmp.jpg";

	ASSERT_TRUE(writer.write(path, img));
	ASSERT_TRUE(std::filesystem::exists(path));

	ImageFileReader reader;
	ASSERT_TRUE(reader.read(path));
	const auto out = reader.toImage();

	EXPECT_EQ(img.getWidth(), out.getWidth());
	EXPECT_EQ(img.getHeight(), out.getHeight());

	std::filesystem::remove(path);
}

TEST(ImageFileReaderTest, ReadWriteTGA)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileReaderTest_tmp.tga";

	ASSERT_TRUE(writer.write(path, img));
	ASSERT_TRUE(std::filesystem::exists(path));

	ImageFileReader reader;
	ASSERT_TRUE(reader.read(path));
	const auto out = reader.toImage();

	compareImagesEqual(img, out);

	std::filesystem::remove(path);
}

TEST(ImageFileReaderTest, ReadWriteDefaultWhenNoExtension)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileReaderTest_tmp_noext";

	ASSERT_TRUE(writer.write(path, img));
	ASSERT_TRUE(std::filesystem::exists(path));

	ImageFileReader reader;
	ASSERT_TRUE(reader.read(path));
	const auto out = reader.toImage();

	compareImagesEqual(img, out);

	std::filesystem::remove(path);
}

TEST(ImageFileReaderTest, ReadFailForNonexistentFile)
{
	ImageFileReader reader;
	const std::string path = "this_file_does_not_exist_hopefully.png";
	EXPECT_FALSE(reader.read(path));
}