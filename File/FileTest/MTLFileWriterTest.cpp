#include "gtest/gtest.h"

#include "../File/MTLFileWriter.h"
#include "../File/MTLFileReader.h"

#include <filesystem>

using namespace Phantom::Graphics;
using namespace Phantom::File;

TEST(MTLFileWriterTest, TestWrite)
{
	MTL m;
	m.ambient = ColorRGBAf(1, 0, 0, 0);
	m.diffuse = ColorRGBAf(0, 1, 0, 0);
	m.specular = ColorRGBAf(0, 0, 1, 0);
	m.ambientTexture = "AmbientTex";
	m.diffuseTexture = "DiffuseTex";
	m.shininessTexture = "ShininessTex";
	m.specularExponent = 1.1f;
	m.name = "Sample";

	MTLFile mtl;
	mtl.materials.push_back(m);

	const auto path = std::filesystem::temp_directory_path() / "MTLFileWriteTest.mtl";
	MTLFileWriter writer;
	EXPECT_TRUE(writer.write(path.string(), mtl));
	std::filesystem::remove(path);
}

TEST(MTLFileWriterTest, TestWriteRoundTripWithTextures)
{
	MTL m;
	m.name = "texMat";
	m.ambientTexture   = "ambient.png";
	m.diffuseTexture   = "diffuse.png";
	m.shininessTexture = "shininess.png";
	m.bumpTexture      = "bump.png";

	MTLFile src;
	src.materials.push_back(m);

	MTLFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.write(ss, src));

	MTLFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getMTL();

	ASSERT_EQ(1u, dst.materials.size());
	const auto& r = dst.materials[0];
	EXPECT_EQ("ambient.png",   r.ambientTexture);
	EXPECT_EQ("diffuse.png",   r.diffuseTexture);
	EXPECT_EQ("shininess.png", r.shininessTexture);
	EXPECT_EQ("bump.png",      r.bumpTexture);
}

TEST(MTLFileWriterTest, TestWriteRoundTripWithIllumination)
{
	MTL m;
	m.name = "illumMat";
	m.illumination = MTL::Illumination::HIGHLIGHT_ON;

	MTLFile src;
	src.materials.push_back(m);

	MTLFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.write(ss, src));

	MTLFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getMTL();

	ASSERT_EQ(1u, dst.materials.size());
	EXPECT_EQ(MTL::Illumination::HIGHLIGHT_ON, dst.materials[0].illumination);
}

TEST(MTLFileWriterTest, TestWriteRoundTripWithOpticalDensity)
{
	MTL m;
	m.name = "glassMat";
	m.opticalDensity = 1.5f;

	MTLFile src;
	src.materials.push_back(m);

	MTLFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.write(ss, src));

	MTLFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getMTL();

	ASSERT_EQ(1u, dst.materials.size());
	EXPECT_FLOAT_EQ(1.5f, dst.materials[0].opticalDensity);
}