#include "pch.h"

#include "../Math/ICurve3d.h"
#include "../Math/ISurface3d.h"
#include "../Math/IVolume3d.h"
#include "../Math/Line2d.h"
#include "../Math/Quaternion.h"
#include "../Math/Statistics.h"
#include "../Math/Vector4d.h"

using namespace Phantom::Math;

namespace {

class Curve3dSample final : public ICurve3df
{
public:
  Vector3df getPosition(const float u) const override
  {
    return Vector3df(u, u * u, u * u * u);
  }
};

class Surface3dSample final : public ISurface3df
{
public:
  Vector3df getPosition(const float u, const float v) const override
  {
    return Vector3df(u, v, u + v);
  }
};

class Volume3dSample final : public IVolume3df
{
public:
  Vector3df getPosition(const float u, const float v, const float w) const override
  {
    return Vector3df(u, v, w);
  }
};

}

TEST(MathProjectMapTest, InterfaceLayerCanBeImplemented)
{
  const Curve3dSample c3;
  const Surface3dSample s3;
  const Volume3dSample v3;

  EXPECT_TRUE(areSame(Vector3df(2.0f, 4.0f, 8.0f), c3.getPosition(2.0f), 1.0e-6f));
  EXPECT_TRUE(areSame(Vector3df(1.0f, 2.0f, 3.0f), s3.getPosition(1.0f, 2.0f), 1.0e-6f));
  EXPECT_TRUE(areSame(Vector3df(1.0f, 2.0f, 3.0f), v3.getPosition(1.0f, 2.0f, 3.0f), 1.0e-6f));
}

TEST(MathProjectMapTest, LinearAlgebraAliasesWorkWithGlm)
{
  const Vector4df v4(1.0f, 2.0f, 3.0f, 4.0f);
  EXPECT_FLOAT_EQ(4.0f, v4.w);

  const Quaternion q = glm::angleAxis(0.25f, Vector3df(0.0f, 1.0f, 0.0f));
  EXPECT_NEAR(1.0f, glm::length(q), 1.0e-6f);
}

TEST(MathProjectMapTest, GeometryAndUtilityCanBeUsedTogether)
{
  const Line2df segment(Vector2df(0.0f, 0.0f), Vector2df(3.0f, 4.0f));
  EXPECT_FLOAT_EQ(5.0f, segment.getLength());

  Statistics<float> stat;
  stat.add(segment.getLength());
  stat.add(7.0f);
  EXPECT_FLOAT_EQ(6.0f, stat.getAverage());
}