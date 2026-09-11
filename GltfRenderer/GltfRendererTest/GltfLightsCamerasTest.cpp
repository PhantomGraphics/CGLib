#include "gtest/gtest.h"

#include "../../File/File/GLTFFileReader.h"
#include "../Gltf/GltfReader.h"
#include "../Gltf/GltfLightsCameras.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Phantom::File;
using namespace Phantom::Gltf;

namespace {

std::filesystem::path writeTempGltf(const char* name, const char* json) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << json;
    return path;
}

// One perspective camera, one point light (KHR_lights_punctual) -- both referenced by a node.
const char* kPerspectiveCameraPointLightGltf = R"JSON(
{
  "asset": { "version": "2.0" },
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {
    "KHR_lights_punctual": {
      "lights": [
        { "type": "point", "color": [1.0, 0.5, 0.25], "intensity": 683.0, "range": 10.0 }
      ]
    }
  },
  "scene": 0,
  "scenes": [ { "nodes": [0, 2] } ],
  "nodes": [
    { "translation": [10.0, 0.0, 0.0], "children": [1] },
    { "translation": [1.0, 2.0, 3.0], "extensions": { "KHR_lights_punctual": { "light": 0 } } },
    { "translation": [5.0, 6.0, 7.0], "camera": 0 }
  ],
  "cameras": [
    { "type": "perspective", "perspective": { "yfov": 1.0, "aspectRatio": 1.5, "znear": 0.1, "zfar": 100.0 } }
  ]
}
)JSON";

// Orthographic camera + spot light (distinct inner/outer cone angles), no node references at all
// -- both definitions should be parsed but collectGltfLightsAndCameras() must report zero
// instances (nothing in the node hierarchy points at them).
const char* kOrthographicCameraSpotLightGltf = R"JSON(
{
  "asset": { "version": "2.0" },
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {
    "KHR_lights_punctual": {
      "lights": [
        { "type": "spot", "color": [1.0, 1.0, 1.0], "intensity": 1000.0,
          "spot": { "innerConeAngle": 0.2, "outerConeAngle": 0.5 } }
      ]
    }
  },
  "scene": 0,
  "scenes": [ { "nodes": [] } ],
  "nodes": [],
  "cameras": [
    { "type": "orthographic", "orthographic": { "xmag": 2.0, "ymag": 3.0, "znear": 0.5, "zfar": 50.0 } }
  ]
}
)JSON";

} // namespace

TEST(GLTFFileReaderCameraLightTest, FileLayerReadsPerspectiveCameraFields)
{
    const auto path = writeTempGltf("GltfLightsCamerasTest_persp.gltf", kPerspectiveCameraPointLightGltf);
    GLTFFileReader reader;
    ASSERT_TRUE(reader.read(path));
    const GLTFFile gltf = reader.getGLTF();

    ASSERT_EQ(1u, gltf.cameras.size());
    const GLTFCamera& cam = gltf.cameras[0];
    EXPECT_EQ("Perspective", cam.type);
    EXPECT_FLOAT_EQ(1.0f, cam.yfov);
    EXPECT_FLOAT_EQ(1.5f, cam.aspectRatio);
    EXPECT_FLOAT_EQ(0.1f, cam.znear);
    EXPECT_FLOAT_EQ(100.0f, cam.zfar);

    std::remove(path.string().c_str());
}

TEST(GLTFFileReaderCameraLightTest, FileLayerReadsPointLightFields)
{
    const auto path = writeTempGltf("GltfLightsCamerasTest_point.gltf", kPerspectiveCameraPointLightGltf);
    GLTFFileReader reader;
    ASSERT_TRUE(reader.read(path));
    const GLTFFile gltf = reader.getGLTF();

    ASSERT_EQ(1u, gltf.lights.size());
    const GLTFLight& light = gltf.lights[0];
    EXPECT_EQ("Point", light.type);
    EXPECT_FLOAT_EQ(1.0f,  light.color[0]);
    EXPECT_FLOAT_EQ(0.5f,  light.color[1]);
    EXPECT_FLOAT_EQ(0.25f, light.color[2]);
    EXPECT_FLOAT_EQ(683.0f, light.intensity);
    EXPECT_FLOAT_EQ(10.0f,  light.range);

    std::remove(path.string().c_str());
}

TEST(GLTFFileReaderCameraLightTest, FileLayerReadsOrthographicCameraAndSpotConeAngles)
{
    const auto path = writeTempGltf("GltfLightsCamerasTest_ortho.gltf", kOrthographicCameraSpotLightGltf);
    GLTFFileReader reader;
    ASSERT_TRUE(reader.read(path));
    const GLTFFile gltf = reader.getGLTF();

    ASSERT_EQ(1u, gltf.cameras.size());
    const GLTFCamera& cam = gltf.cameras[0];
    EXPECT_EQ("Orthographic", cam.type);
    EXPECT_FLOAT_EQ(2.0f, cam.xmag);
    EXPECT_FLOAT_EQ(3.0f, cam.ymag);
    EXPECT_FLOAT_EQ(0.5f, cam.znear);
    EXPECT_FLOAT_EQ(50.0f, cam.zfar);

    ASSERT_EQ(1u, gltf.lights.size());
    const GLTFLight& light = gltf.lights[0];
    EXPECT_EQ("Spot", light.type);
    EXPECT_FLOAT_EQ(0.2f, light.innerConeAngle);
    EXPECT_FLOAT_EQ(0.5f, light.outerConeAngle);

    std::remove(path.string().c_str());
}

TEST(GltfLightsCamerasTest, CollectsWorldTransformFromNestedLightNode)
{
    // Node 1 (the light) is a child of node 0 (translated (10,0,0)) and itself translates
    // (1,2,3) -- the world position must be the composed (11,2,3), not just the local (1,2,3).
    const auto path = writeTempGltf("GltfLightsCamerasTest_world.gltf", kPerspectiveCameraPointLightGltf);
    const auto doc = GltfReader::load(path);
    ASSERT_TRUE(doc.has_value());

    const GltfSceneLightsCameras lc = collectGltfLightsAndCameras(*doc);
    ASSERT_EQ(1u, lc.lights.size());
    EXPECT_EQ(0, lc.lights[0].lightIndex);
    const glm::vec3 lightPos = glm::vec3(lc.lights[0].worldMatrix[3]);
    EXPECT_FLOAT_EQ(11.0f, lightPos.x);
    EXPECT_FLOAT_EQ(2.0f,  lightPos.y);
    EXPECT_FLOAT_EQ(3.0f,  lightPos.z);

    ASSERT_EQ(1u, lc.cameras.size());
    EXPECT_EQ(0, lc.cameras[0].cameraIndex);
    const glm::vec3 camPos = glm::vec3(lc.cameras[0].worldMatrix[3]);
    EXPECT_FLOAT_EQ(5.0f, camPos.x);
    EXPECT_FLOAT_EQ(6.0f, camPos.y);
    EXPECT_FLOAT_EQ(7.0f, camPos.z);

    std::remove(path.string().c_str());
}

TEST(GltfLightsCamerasTest, UnreferencedDefinitionsYieldNoInstances)
{
    // The orthographic camera and spot light in this fixture exist as *definitions* but no scene
    // node references either -- collectGltfLightsAndCameras() must report zero instances of each
    // (this is exactly the case ImportReport's "unused definition" note covers).
    const auto path = writeTempGltf("GltfLightsCamerasTest_unused.gltf", kOrthographicCameraSpotLightGltf);
    const auto doc = GltfReader::load(path);
    ASSERT_TRUE(doc.has_value());
    EXPECT_EQ(1u, doc->cameras.size());
    EXPECT_EQ(1u, doc->lights.size());

    const GltfSceneLightsCameras lc = collectGltfLightsAndCameras(*doc);
    EXPECT_TRUE(lc.lights.empty());
    EXPECT_TRUE(lc.cameras.empty());

    std::remove(path.string().c_str());
}
