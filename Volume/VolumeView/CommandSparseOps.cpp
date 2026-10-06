#include "CommandDispatcher.h"
#include "CommandParse.h"

#include "VolumeScene.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Coord.h"
#include "../Volume/SparseVolumeTree/Interpolator.h"
#include "../Volume/LevelSet.h"
#include "../Volume/MCSurfaceBuilder.h"

#define GLM_FORCE_RADIANS
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

// Sparse-volume operations (CreateSphere / CreateBox / CSGCombine / Resample / MarchingCubes).
// Argument parsing lives in CommandDispatcher::route(); these only act on the World.

namespace VolumeView {

std::string CommandDispatcher::cmdCreateSphere(
    float cx, float cy, float cz, float radius, float cell)
{
    if (!world_) return "Error:no world";

    const float r    = std::max(radius, 0.01f);
    const float cs   = std::max(cell, 0.05f);
    const float band = cs * 3.0f;

    auto volume = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
    volume->setVoxelSize(cs);

    const glm::vec3 center(cx, cy, cz);

    const int iLo = static_cast<int>(std::floor((cx - (r + band)) / cs));
    const int iHi = static_cast<int>(std::ceil ((cx + (r + band)) / cs));
    const int jLo = static_cast<int>(std::floor((cy - (r + band)) / cs));
    const int jHi = static_cast<int>(std::ceil ((cy + (r + band)) / cs));
    const int kLo = static_cast<int>(std::floor((cz - (r + band)) / cs));
    const int kHi = static_cast<int>(std::ceil ((cz + (r + band)) / cs));

    for (int i = iLo; i <= iHi; ++i) {
        for (int j = jLo; j <= jHi; ++j) {
            for (int k = kLo; k <= kHi; ++k) {
                const glm::vec3 wp(i * cs, j * cs, k * cs);
                const float sdf = glm::distance(wp, center) - r;
                if (std::abs(sdf) <= band)
                    volume->setValue(Phantom::Volume::Coord(i, j, k), sdf);
            }
        }
    }

    const std::string name = "Sphere_" + std::to_string(opCount_++);
    auto* scene = world_->addScene(name);
    scene->setShape(std::move(volume));
    const int voxels = scene->getShape()->getActiveVoxelCount();
    if (pActiveSceneId_) *pActiveSceneId_ = scene->getId();
    if (onRebuild_) onRebuild_();
    return "OK:" + name + ":" + std::to_string(voxels) + "vx";
}

std::string CommandDispatcher::cmdCreateBox(
    float minX, float minY, float minZ,
    float maxX, float maxY, float maxZ, float cell)
{
    if (!world_) return "Error:no world";

    const float cs = std::max(cell, 0.05f);
    const Phantom::Math::Box3df box(
        Phantom::Math::Vector3df(std::min(minX, maxX),
                                 std::min(minY, maxY),
                                 std::min(minZ, maxZ)),
        Phantom::Math::Vector3df(std::max(minX, maxX),
                                 std::max(minY, maxY),
                                 std::max(minZ, maxZ)));

    auto volume = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
    volume->setVoxelSize(cs);

    Phantom::Volume::LevelSet levelSet;
    levelSet.setSignedDistance(box, *volume, static_cast<double>(cs) * 3.0);

    const std::string name = "Box_" + std::to_string(opCount_++);
    auto* scene = world_->addScene(name);
    scene->setShape(std::move(volume));
    const int voxels = scene->getShape()->getActiveVoxelCount();
    if (pActiveSceneId_) *pActiveSceneId_ = scene->getId();
    if (onRebuild_) onRebuild_();
    return "OK:" + name + ":" + std::to_string(voxels) + "vx";
}

std::string CommandDispatcher::cmdCsgCombine(
    const std::string& op, int idxA, int idxB)
{
    if (!world_) return "Error:no world";

    const auto& scenes = world_->getScenes();
    if (idxA < 0 || idxA >= static_cast<int>(scenes.size()) ||
        idxB < 0 || idxB >= static_cast<int>(scenes.size()))
        return "Error:CSGCombine index out of range";
    if (idxA == idxB) return "Error:CSGCombine idxA == idxB";

    const auto* shapeA = scenes[static_cast<size_t>(idxA)]->getShape();
    const auto* shapeB = scenes[static_cast<size_t>(idxB)]->getShape();
    if (!shapeA || !shapeB) return "Error:CSGCombine scene has no shape";

    // Determine operation from string.
    // 0=Union, 1=Intersection, 2=Difference
    int opCode = -1;
    if (op == "Union")        opCode = 0;
    else if (op == "Intersection") opCode = 1;
    else if (op == "Difference")   opCode = 2;
    if (opCode < 0) return "Error:unknown CSG op '" + op + "'";

    static const char* kOpNames[] = { "union", "inter", "diff" };

    const float cellSize = shapeA->getVoxelSize();
    auto result = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
    result->setVoxelSize(cellSize);

    const auto bboxA = shapeA->getBoundingBox();
    const auto bboxB = shapeB->getBoundingBox();
    const Phantom::Math::Vector3df wMin(
        std::min(bboxA.getMin().x, bboxB.getMin().x),
        std::min(bboxA.getMin().y, bboxB.getMin().y),
        std::min(bboxA.getMin().z, bboxB.getMin().z));
    const Phantom::Math::Vector3df wMax(
        std::max(bboxA.getMax().x, bboxB.getMax().x),
        std::max(bboxA.getMax().y, bboxB.getMax().y),
        std::max(bboxA.getMax().z, bboxB.getMax().z));

    const Phantom::Volume::Coord iMin = result->worldToIndex(wMin);
    const Phantom::Volume::Coord iMax = result->worldToIndex(wMax);

    Phantom::Volume::TrilinearInterpolator<float> interpA(*shapeA);
    Phantom::Volume::TrilinearInterpolator<float> interpB(*shapeB);
    constexpr float kBg = 1e6f;

    for (int i = iMin.x - 1; i <= iMax.x + 1; ++i) {
        for (int j = iMin.y - 1; j <= iMax.y + 1; ++j) {
            for (int k = iMin.z - 1; k <= iMax.z + 1; ++k) {
                const Phantom::Volume::Coord idx(i, j, k);
                const auto  wp = result->indexToWorld(idx);
                const float a  = interpA.getValue(wp);
                const float b  = interpB.getValue(wp);

                float val = kBg;
                switch (opCode) {
                case 0: val = std::min(a, b);  break; // Union
                case 1: val = std::max(a, b);  break; // Intersection
                case 2: val = std::max(a, -b); break; // Difference
                }

                if (val < kBg * 0.99f)
                    result->setValue(idx, val);
            }
        }
    }

    const std::string name =
        scenes[static_cast<size_t>(idxA)]->getName() + "_" +
        kOpNames[opCode] + "_" +
        scenes[static_cast<size_t>(idxB)]->getName() + "_" +
        std::to_string(opCount_++);
    auto* scene = world_->addScene(name);
    scene->setShape(std::move(result));
    const int voxels = scene->getShape()->getActiveVoxelCount();
    if (pActiveSceneId_) *pActiveSceneId_ = scene->getId();
    if (onRebuild_) onRebuild_();
    return "OK:" + name + ":" + std::to_string(voxels) + "vx";
}

std::string CommandDispatcher::cmdResample(float newCell) {
    if (!world_ || !pActiveSceneId_) return "Error:not initialized";

    const auto* src = world_->findById(*pActiveSceneId_);
    if (!src || !src->getShape()) return "Error:no active volume scene";

    const auto* shape = src->getShape();
    const float cs    = std::max(newCell, 0.05f);

    auto result = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
    result->setVoxelSize(cs);

    const auto bbox = shape->getBoundingBox();
    const Phantom::Volume::Coord iMin = result->worldToIndex(bbox.getMin());
    const Phantom::Volume::Coord iMax = result->worldToIndex(bbox.getMax());

    Phantom::Volume::TrilinearInterpolator<float> interp(*shape);
    const float bg = shape->getBackground();

    for (int i = iMin.x - 1; i <= iMax.x + 1; ++i) {
        for (int j = iMin.y - 1; j <= iMax.y + 1; ++j) {
            for (int k = iMin.z - 1; k <= iMax.z + 1; ++k) {
                const Phantom::Volume::Coord idx(i, j, k);
                const auto  wp  = result->indexToWorld(idx);
                const float val = interp.getValue(wp);
                if (std::fabs(val) < bg * 0.99f)
                    result->setValue(idx, val);
            }
        }
    }

    const std::string name = src->getName() + "_resamp_" + std::to_string(opCount_++);
    auto* scene = world_->addScene(name);
    scene->setShape(std::move(result));
    const int voxels = scene->getShape()->getActiveVoxelCount();
    if (onRebuild_) onRebuild_();
    return "OK:" + std::to_string(voxels) + "vx";
}

std::string CommandDispatcher::cmdMarchingCubes(float isoLevel) {
    if (!world_ || !pActiveSceneId_) return "Error:not initialized";

    const auto* src = world_->findById(*pActiveSceneId_);
    if (!src || !src->getShape()) return "Error:no active volume scene";

    if (src->getShape()->getActiveVoxelCount() == 0) return "Error:empty sparse volume";

    Phantom::Volume::MCSurfaceBuilder builder;
    builder.build(*src->getShape(), isoLevel);
    const auto& tris = builder.getTriangles();

    PolygonMesh mesh;
    mesh.name = "MC_" + src->getName();
    mesh.positions.reserve(tris.size() * 9);
    mesh.colors.reserve(tris.size() * 12);

    uint32_t idx = 0;
    for (const auto& tri : tris) {
        const auto& verts = tri.getVertices();
        for (int vi = 0; vi < 3; ++vi) {
            mesh.positions.push_back(static_cast<float>(verts[vi].x));
            mesh.positions.push_back(static_cast<float>(verts[vi].y));
            mesh.positions.push_back(static_cast<float>(verts[vi].z));
            mesh.colors.push_back(0.8f);
            mesh.colors.push_back(0.8f);
            mesh.colors.push_back(0.9f);
            mesh.colors.push_back(0.85f);
            mesh.indices.push_back(idx++);
        }
    }

    world_->clearPolygons();
    world_->addPolygon(std::move(mesh));
    if (onRebuild_) onRebuild_();
    return "OK:" + std::to_string(tris.size()) + "tri";
}

} // namespace VolumeView
