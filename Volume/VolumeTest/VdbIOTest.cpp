#include "pch.h"

#include "../Volume/SparseVolumeTree/VdbWriter.h"
#include "../Volume/SparseVolumeTree/VdbReader.h"

#include <cmath>
#include <cstdio>
#include <fstream>

using namespace Phantom::Math;
using namespace Phantom::Volume;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string writeTempVdb(const std::string& tag, const SparseVolumef& vol,
                                const std::string& gridName = "density")
{
    const std::string path = "test_vdbio_" + tag + ".vdb";
    SparseVolumeVdbWriter writer;
    EXPECT_TRUE(writer.write(path, vol, gridName));
    return path;
}

// ---------------------------------------------------------------------------
// Writer tests
// ---------------------------------------------------------------------------

TEST(VdbWriterTest, MagicNumberInHeader)
{
    SparseVolumef sv;
    sv.setValue(Coord(0, 0, 0), 1.0f);
    sv.setValue(Coord(1, 2, 3), 0.5f);

    const std::string path = writeTempVdb("magic", sv);

    std::ifstream ifs(path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());

    int32_t magic = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0x56444220);

    ifs.close();
    ::remove(path.c_str());
}

TEST(VdbWriterTest, NonEmptyOutputForActiveVoxels)
{
    SparseVolumef sv;
    sv.setValue(Coord(0,   0,   0),   1.0f);
    sv.setValue(Coord(100, 0,   0),   2.0f);
    sv.setValue(Coord(0,   100, 0),   3.0f);
    sv.setValue(Coord(0,   0,   100), 4.0f);

    const std::string path = writeTempVdb("nonempty", sv);

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(ifs.is_open());
    EXPECT_GT(static_cast<int64_t>(ifs.tellg()), 200);

    ifs.close();
    ::remove(path.c_str());
}

TEST(VdbWriterTest, EmptyVolumeWritesValidHeader)
{
    SparseVolumef sv;
    const std::string path = writeTempVdb("empty", sv);

    std::ifstream ifs(path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());

    int32_t magic = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0x56444220);

    ifs.close();
    ::remove(path.c_str());
}

TEST(VdbWriterTest, NegativeCoordinates)
{
    SparseVolumef sv;
    sv.setValue(Coord(-10, -20, -30), 7.0f);
    sv.setValue(Coord( 10,  20,  30), 3.0f);

    const std::string path = writeTempVdb("negcoords", sv);

    std::ifstream ifs(path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());

    int32_t magic = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0x56444220);

    ifs.close();
    ::remove(path.c_str());
}

TEST(VdbWriterTest, LargeScatterWritesFile)
{
    SparseVolumef sv;
    for (int i = 0; i < 10; ++i)
        sv.setValue(Coord(i * 200, i * 150, i * 100), static_cast<float>(i) * 0.1f);

    const std::string path = writeTempVdb("scatter", sv);

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(ifs.is_open());
    EXPECT_GT(static_cast<int64_t>(ifs.tellg()), 0);

    ifs.close();
    ::remove(path.c_str());
}

TEST(VdbWriterTest, CustomGridName)
{
    SparseVolumef sv;
    sv.setValue(Coord(0, 0, 0), 5.0f);

    SparseVolumeVdbWriter writer;
    const std::string path = "test_vdbio_gridname.vdb";
    EXPECT_TRUE(writer.write(path, sv, "temperature"));

    std::ifstream ifs(path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    ifs.close();
    ::remove(path.c_str());
}

TEST(VdbWriterTest, VoxelSizeWrittenWithoutError)
{
    SparseVolumef sv;
    sv.setVoxelSize(0.1f);
    sv.setValue(Coord(0, 0, 0), 1.0f);
    sv.setValue(Coord(7, 7, 7), 2.0f);

    const std::string path = writeTempVdb("voxelsize", sv);

    std::ifstream ifs(path, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());

    int32_t magic = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0x56444220);

    ifs.close();
    ::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// Reader (roundtrip) tests
// ---------------------------------------------------------------------------

TEST(VdbReaderTest, SingleVoxelRoundtrip)
{
    SparseVolumef original(1e6f);
    original.setVoxelSize(0.5f);
    original.setValue(Coord(1, 2, 3), 42.0f);

    const auto path = writeTempVdb("single", original);

    SparseVolumeVdbReader reader;
    auto result = reader.read(path);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->getActiveVoxelCount(), 1);
    EXPECT_FLOAT_EQ(result->getValue(Coord(1, 2, 3)), 42.0f);
    EXPECT_FLOAT_EQ(result->getVoxelSize(), 0.5f);

    ::remove(path.c_str());
}

TEST(VdbReaderTest, MultipleVoxelsRoundtrip)
{
    SparseVolumef original(1e6f);
    original.setVoxelSize(1.0f);
    original.setValue(Coord(0,   0,   0),   1.0f);
    original.setValue(Coord(100, 0,   0),   2.0f);
    original.setValue(Coord(0,   100, 0),   3.0f);
    original.setValue(Coord(0,   0,   100), 4.0f);

    const auto path = writeTempVdb("multi", original);

    SparseVolumeVdbReader reader;
    auto result = reader.read(path);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->getActiveVoxelCount(), original.getActiveVoxelCount());
    EXPECT_FLOAT_EQ(result->getValue(Coord(0,   0,   0)),   1.0f);
    EXPECT_FLOAT_EQ(result->getValue(Coord(100, 0,   0)),   2.0f);
    EXPECT_FLOAT_EQ(result->getValue(Coord(0,   100, 0)),   3.0f);
    EXPECT_FLOAT_EQ(result->getValue(Coord(0,   0,   100)), 4.0f);

    ::remove(path.c_str());
}

TEST(VdbReaderTest, NegativeCoordinatesRoundtrip)
{
    SparseVolumef original(1e6f);
    original.setVoxelSize(2.0f);
    original.setValue(Coord(-10, -20, -30), 7.0f);
    original.setValue(Coord( 10,  20,  30), 3.0f);

    const auto path = writeTempVdb("negcoords", original);

    SparseVolumeVdbReader reader;
    auto result = reader.read(path);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->getActiveVoxelCount(), 2);
    EXPECT_FLOAT_EQ(result->getValue(Coord(-10, -20, -30)), 7.0f);
    EXPECT_FLOAT_EQ(result->getValue(Coord( 10,  20,  30)), 3.0f);
    EXPECT_FLOAT_EQ(result->getVoxelSize(), 2.0f);

    ::remove(path.c_str());
}

TEST(VdbReaderTest, VoxelSizePreserved)
{
    SparseVolumef original(1e6f);
    original.setVoxelSize(0.1f);
    original.setValue(Coord(0, 0, 0), 1.0f);
    original.setValue(Coord(7, 7, 7), 2.0f);

    const auto path = writeTempVdb("voxelsize2", original);

    SparseVolumeVdbReader reader;
    auto result = reader.read(path);
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(result->getVoxelSize(), 0.1f, 1e-5f);

    ::remove(path.c_str());
}

TEST(VdbReaderTest, InvalidFileReturnsNull)
{
    SparseVolumeVdbReader reader;
    const auto result = reader.read("nonexistent_file_xyz_99999.vdb");
    EXPECT_EQ(result, nullptr);
}

TEST(VdbReaderTest, SphereSdfRoundtrip)
{
    SparseVolumef original(1e6f);
    original.setVoxelSize(1.0f);
    const float radius = 10.0f;
    const float band   = 3.0f;
    for (int i = -14; i <= 14; ++i)
        for (int j = -14; j <= 14; ++j)
            for (int k = -14; k <= 14; ++k) {
                const float d = std::sqrt(float(i*i + j*j + k*k)) - radius;
                if (std::fabs(d) <= band)
                    original.setValue(Coord(i, j, k), d);
            }

    const int origCount = original.getActiveVoxelCount();
    ASSERT_GT(origCount, 0);

    const auto path = writeTempVdb("sphere_sdf", original, "sdf");

    SparseVolumeVdbReader reader;
    auto result = reader.read(path);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->getActiveVoxelCount(), origCount);
    EXPECT_FLOAT_EQ(result->getVoxelSize(), 1.0f);
    EXPECT_NEAR(result->getValue(Coord(10, 0, 0)), 0.0f, 1.0f);
    EXPECT_LT(result->getValue(Coord(8, 0, 0)), 0.0f);
    EXPECT_GT(result->getValue(Coord(12, 0, 0)), 0.0f);

    ::remove(path.c_str());
}

TEST(VdbReaderTest, BackgroundValuePreserved)
{
    const float customBg = -999.0f;
    SparseVolumef original(customBg);
    original.setVoxelSize(1.0f);
    original.setValue(Coord(1, 0, 0), 5.0f);
    original.setValue(Coord(2, 0, 0), 6.0f);

    const auto path = writeTempVdb("bgval", original);

    SparseVolumeVdbReader reader;
    auto result = reader.read(path);
    ASSERT_NE(result, nullptr);

    EXPECT_FLOAT_EQ(result->getBackground(), customBg);
    // 未設定ボクセルはカスタム背景値を返す
    EXPECT_FLOAT_EQ(result->getValue(Coord(99, 99, 99)), customBg);

    ::remove(path.c_str());
}

TEST(VdbReaderTest, AllValuesMatchOriginal)
{
    SparseVolumef original(1e6f);
    original.setVoxelSize(1.0f);
    int idx = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                original.setValue(Coord(i, j, k), static_cast<float>(++idx));

    const auto path = writeTempVdb("allvals", original);

    SparseVolumeVdbReader reader;
    auto result = reader.read(path);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->getActiveVoxelCount(), original.getActiveVoxelCount());

    idx = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                EXPECT_FLOAT_EQ(result->getValue(Coord(i, j, k)), static_cast<float>(++idx));

    ::remove(path.c_str());
}
