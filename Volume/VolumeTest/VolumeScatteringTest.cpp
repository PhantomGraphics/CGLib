#include "pch.h"

#include "../Volume/VolumeScattering.h"

#include <cmath>

using namespace Phantom::Math;
using namespace Phantom::Volume;
namespace VS = Phantom::Volume::VolumeScattering;

namespace
{
// 16^3 grid, cell 1. Density `rho` fills every x,y column but only z in [4, 12) (8 cells thick).
ScalarGrid3D makeSlab(float rho)
{
  ScalarGridDesc d;
  d.nx = d.ny = d.nz = 16;
  d.cellSize = 1.0f;
  ScalarGrid3D g(d, 0.0f, 0.0f);
  for (uint32_t k = 4; k < 12; ++k)
    for (uint32_t j = 0; j < 16; ++j)
      for (uint32_t i = 0; i < 16; ++i) g.at(i, j, k) = rho;
  return g;
}

ScalarGrid3D ones(const ScalarGridDesc& d) { return ScalarGrid3D(d, 1.0f, 1.0f); }

ScalarGrid3D makeBlob(uint32_t n, float peak)
{
  ScalarGridDesc d;
  d.nx = d.ny = d.nz = n;
  d.cellSize = 1.0f;
  ScalarGrid3D g(d);
  const float c = 0.5f * static_cast<float>(n), r = 0.35f * static_cast<float>(n);
  for (uint32_t k = 0; k < n; ++k)
    for (uint32_t j = 0; j < n; ++j)
      for (uint32_t i = 0; i < n; ++i) {
        const Vector3df p = g.cellCenter(i, j, k) - Vector3df(c);
        const float q = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z) / r;
        g.at(i, j, k) = q < 1.0f ? peak * (1.0f - q * q) : 0.0f;
      }
  return g;
}
}

TEST(VolumeScatteringTest, HenyeyGreensteinIsNormalized)
{
  for (float g : { -0.5f, 0.0f, 0.6f, 0.9f }) {
    // integral over the sphere of p(cos) = 2*pi * integral_{-1}^{1} p(c) dc = 1
    double sum = 0.0;
    const int n = 200000;
    for (int i = 0; i < n; ++i) {
      const float c = -1.0f + 2.0f * (i + 0.5f) / n;
      sum += VS::henyeyGreenstein(c, g) * (2.0 / n);
    }
    EXPECT_NEAR(sum * 2.0 * 3.14159265358979, 1.0, 2.0e-3) << "g=" << g;
  }
  EXPECT_GT(VS::henyeyGreenstein(1.0f, 0.6f), VS::henyeyGreenstein(-1.0f, 0.6f));
}

TEST(VolumeScatteringTest, IntersectBox)
{
  float t0 = 0, t1 = 0;
  EXPECT_TRUE(VS::intersectBox(Vector3df(0, 0, -5), Vector3df(0, 0, 1), Vector3df(-1), Vector3df(1), t0, t1));
  EXPECT_NEAR(t0, 4.0f, 1e-5f);
  EXPECT_NEAR(t1, 6.0f, 1e-5f);
  EXPECT_FALSE(VS::intersectBox(Vector3df(5, 0, -5), Vector3df(0, 0, 1), Vector3df(-1), Vector3df(1), t0, t1));
  EXPECT_FALSE(VS::intersectBox(Vector3df(0, 0, 5), Vector3df(0, 0, 1), Vector3df(-1), Vector3df(1), t0, t1));  // behind
  EXPECT_TRUE(VS::intersectBox(Vector3df(0), Vector3df(0, 0, 1), Vector3df(-1), Vector3df(1), t0, t1));          // inside
  EXPECT_EQ(t0, 0.0f);
}

TEST(VolumeScatteringTest, UniformSlabTransmittanceMatchesBeerLambert)
{
  const ScalarGrid3D rho = makeSlab(1.0f);
  ScatteringParams p;
  p.extinction = 0.1f;
  p.minTransmittance = 0.0f;
  const ScalarGrid3D lit = ones(rho.desc());
  for (float step : { 0.25f, 0.5f, 1.0f, 2.0f }) {
    p.stepLength = step;
    const RayResult r = VS::marchRay(rho, lit, p, Vector3df(8.0f, 8.0f, -3.0f), Vector3df(0, 0, 1));
    EXPECT_NEAR(r.transmittance, std::exp(-0.8f), 2.0e-3f) << "step=" << step;
  }
}

TEST(VolumeScatteringTest, SlabRadianceMatchesAnalyticSingleScatter)
{
  const ScalarGrid3D rho = makeSlab(1.0f);
  ScatteringParams p;
  p.extinction = 0.2f;
  p.albedo = 0.8f;
  p.phaseG = 0.0f;
  p.sunIrradiance = 3.0f;
  p.ambient = 0.5f;
  p.stepLength = 0.5f;
  p.minTransmittance = 0.0f;
  p.sunDirection = Vector3df(0, 1, 0);
  const ScalarGrid3D lit = ones(rho.desc());   // no self-shadowing: source is constant along the ray
  const RayResult r = VS::marchRay(rho, lit, p, Vector3df(8.0f, 8.0f, -3.0f), Vector3df(0, 0, 1));
  const float src = p.albedo * (p.sunIrradiance * VS::henyeyGreenstein(0.0f, 0.0f) + p.ambient);
  const float T = std::exp(-0.2f * 8.0f);
  EXPECT_NEAR(r.radiance.x, src * (1.0f - T), 2.0e-3f);   // L = src * (1 - T)
  EXPECT_NEAR(r.radiance.x, r.radiance.z, 1e-6f);
  EXPECT_NEAR(r.transmittance, T, 2.0e-3f);
}

TEST(VolumeScatteringTest, SunTransmittanceInsideSlab)
{
  const ScalarGrid3D rho = makeSlab(1.0f);
  ScatteringParams p;
  p.extinction = 0.1f;
  p.sunDirection = Vector3df(0, 0, 1);
  p.stepLength = 0.25f;
  const ScalarGrid3D t = VS::computeSunTransmittance(rho, p);
  EXPECT_EQ(t.borderValue(), 1.0f);
  EXPECT_NEAR(t.at(8, 8, 15), 1.0f, 1e-6f);                       // above the slab
  EXPECT_NEAR(t.at(8, 8, 11), std::exp(-0.1f * 0.5f), 5.0e-3f);   // half a cell below the top plane (z=12)
  EXPECT_NEAR(t.at(8, 8, 4), std::exp(-0.1f * 7.5f), 5.0e-3f);    // bottom cell of the slab
  EXPECT_NEAR(t.at(8, 8, 0), std::exp(-0.1f * 8.0f), 5.0e-3f);    // below the slab: whole thickness
  // Monotone: deeper cells see less sun.
  for (uint32_t k = 1; k < 16; ++k) EXPECT_GE(t.at(8, 8, k) + 1e-6f, t.at(8, 8, k - 1));
}

TEST(VolumeScatteringTest, EmptyMissAndZeroAlbedo)
{
  ScalarGridDesc d;
  d.nx = d.ny = d.nz = 8;
  ScalarGrid3D empty(d);
  ScatteringParams p;
  const ScalarGrid3D lit = ones(d);
  RayResult r = VS::marchRay(empty, lit, p, Vector3df(4, 4, -3), Vector3df(0, 0, 1));
  EXPECT_EQ(r.radiance.x, 0.0f);
  EXPECT_EQ(r.transmittance, 1.0f);
  r = VS::marchRay(empty, lit, p, Vector3df(40, 4, -3), Vector3df(0, 0, 1));   // misses the box
  EXPECT_EQ(r.steps, 0);
  EXPECT_EQ(r.transmittance, 1.0f);

  const ScalarGrid3D blob = makeBlob(16, 2.0f);
  p.albedo = 0.0f;
  r = VS::marchRay(blob, ones(blob.desc()), p, Vector3df(8, 8, -3), Vector3df(0, 0, 1));
  EXPECT_EQ(r.radiance.x, 0.0f);
  EXPECT_LT(r.transmittance, 1.0f);   // still attenuates
}

TEST(VolumeScatteringTest, TMaxTruncatesTheRay)
{
  const ScalarGrid3D rho = makeSlab(1.0f);
  ScatteringParams p;
  p.extinction = 0.1f;
  p.stepLength = 0.5f;
  p.minTransmittance = 0.0f;
  const ScalarGrid3D lit = ones(rho.desc());
  // Ray enters the box at z=0 (t=3); an opaque surface at t=3+8 (z=8) leaves 4 cells of slab (z 4..8).
  const RayResult r = VS::marchRay(rho, lit, p, Vector3df(8, 8, -3), Vector3df(0, 0, 1), 11.0f);
  EXPECT_NEAR(r.transmittance, std::exp(-0.4f), 3.0e-3f);
}

TEST(VolumeScatteringTest, SelfShadowingDarkensTheBlobAndStepConverges)
{
  const ScalarGrid3D blob = makeBlob(24, 3.0f);
  ScatteringParams p;
  p.extinction = 0.4f;
  p.phaseG = 0.0f;
  p.sunDirection = Vector3df(0, 0, 1);
  p.minTransmittance = 0.0f;
  p.stepLength = 0.125f;
  const ScalarGrid3D shadowed = VS::computeSunTransmittance(blob, p);
  const Vector3df o(12, 12, -3);
  const Vector3df dir(1.0f, 0.0f, 0.0f);   // side-on view of the sun-lit blob
  const Vector3df side(-3.0f, 12.0f, 12.0f);
  const RayResult withShadow = VS::marchRay(blob, shadowed, p, side, dir);
  const RayResult noShadow = VS::marchRay(blob, ones(blob.desc()), p, side, dir);
  (void)o;
  EXPECT_LT(withShadow.radiance.x, noShadow.radiance.x);
  EXPECT_GT(withShadow.radiance.x, 0.0f);

  ScatteringParams coarse = p;
  coarse.stepLength = 0.5f;
  const RayResult c = VS::marchRay(blob, VS::computeSunTransmittance(blob, coarse), coarse, side, dir);
  EXPECT_NEAR(c.radiance.x, withShadow.radiance.x, 0.03f * withShadow.radiance.x);
  EXPECT_NEAR(c.transmittance, withShadow.transmittance, 0.02f);
}

TEST(VolumeScatteringTest, MaxStepsCapIsReported)
{
  const ScalarGrid3D rho = makeSlab(0.01f);   // thin: never reaches minTransmittance
  ScatteringParams p;
  p.stepLength = 0.1f;
  p.maxSteps = 10;
  const RayResult r = VS::marchRay(rho, ones(rho.desc()), p, Vector3df(8, 8, -3), Vector3df(0, 0, 1));
  EXPECT_TRUE(r.hitCap);
  EXPECT_EQ(r.steps, 10);
}

TEST(ScalarGrid3DTest, TrilinearSampling)
{
  ScalarGridDesc d;
  d.nx = d.ny = d.nz = 2;
  ScalarGrid3D g(d, 0.0f, 0.0f);
  g.at(0, 0, 0) = 1.0f;
  EXPECT_NEAR(g.sample(g.cellCenter(0, 0, 0)), 1.0f, 1e-6f);
  EXPECT_NEAR(g.sample(g.cellCenter(1, 0, 0)), 0.0f, 1e-6f);
  EXPECT_NEAR(g.sample(Vector3df(1.0f, 0.5f, 0.5f)), 0.5f, 1e-6f);   // midway between two centres
  EXPECT_NEAR(g.sample(Vector3df(-5, 0, 0)), 0.0f, 1e-6f);           // border value
  ScalarGrid3D b(d, 1.0f, 7.0f);
  EXPECT_NEAR(b.sample(Vector3df(-5, 0, 0)), 7.0f, 1e-6f);
}
