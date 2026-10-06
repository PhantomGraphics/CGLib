#include "CommandDispatcher.h"
#include "CommandParse.h"

#include "VolumeScene.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Coord.h"
#include "../Volume/MCSurfaceBuilder.h"
#include "../Volume/Volume.h"

#include <algorithm>
#include <array>
#include <climits>
#include <memory>
#include <string>

// Dense-volume operations (CreateDenseBox / DenseFromSparse / DenseMarchingCubes / DeleteDense).
// Argument parsing lives in CommandDispatcher::route(); these only act on the World.

namespace VolumeView {

namespace {

Phantom::Volume::Volumef sparseToVolumeWithVoxelSize(
    const Phantom::Volume::SparseVolumef& sparse,
    float voxelSize)
{
    if (sparse.getActiveVoxelCount() == 0)
        return Phantom::Volume::Volumef{};

    int minX = INT_MAX, minY = INT_MAX, minZ = INT_MAX;
    int maxX = INT_MIN, maxY = INT_MIN, maxZ = INT_MIN;

    sparse.forEachActive([&](const Phantom::Volume::Coord& c,
                             const Phantom::Math::Vector3df&, float) {
        minX = std::min(minX, (int)c.x); minY = std::min(minY, (int)c.y);
        minZ = std::min(minZ, (int)c.z);
        maxX = std::max(maxX, (int)c.x); maxY = std::max(maxY, (int)c.y);
        maxZ = std::max(maxZ, (int)c.z);
    });

    const int ox = minX - 1, oy = minY - 1, oz = minZ - 1;
    const size_t dimX = static_cast<size_t>(maxX - minX + 3);
    const size_t dimY = static_cast<size_t>(maxY - minY + 3);
    const size_t dimZ = static_cast<size_t>(maxZ - minZ + 3);

    const float vs = std::max(voxelSize, 0.05f);
    const Phantom::Math::Vector3df bMin(
        (static_cast<float>(ox) - 0.5f) * vs,
        (static_cast<float>(oy) - 0.5f) * vs,
        (static_cast<float>(oz) - 0.5f) * vs);
    const Phantom::Math::Vector3df bMax(
        bMin.x + static_cast<float>(dimX) * vs,
        bMin.y + static_cast<float>(dimY) * vs,
        bMin.z + static_cast<float>(dimZ) * vs);

    Phantom::Volume::Volumef dense(
        Phantom::Math::Box3df(bMin, bMax), {dimX, dimY, dimZ});

    const float bg = sparse.getBackground();
    for (size_t i = 0; i < dimX; ++i)
        for (size_t j = 0; j < dimY; ++j)
            for (size_t k = 0; k < dimZ; ++k)
                dense.setValue({(int)i, (int)j, (int)k}, bg);

    sparse.forEachActive([&](const Phantom::Volume::Coord& c,
                             const Phantom::Math::Vector3df&, float value) {
        const size_t di = static_cast<size_t>(c.x - ox);
        const size_t dj = static_cast<size_t>(c.y - oy);
        const size_t dk = static_cast<size_t>(c.z - oz);
        dense.setValue({(int)di, (int)dj, (int)dk}, value);
    });

    return dense;
}
} // anonymous namespace

std::string CommandDispatcher::cmdCreateDenseBox(
    float minX, float minY, float minZ,
    float maxX, float maxY, float maxZ,
    int resX, int resY, int resZ)
{
    if (!world_) return "Error:no world";

    const float x0 = std::min(minX, maxX);
    const float y0 = std::min(minY, maxY);
    const float z0 = std::min(minZ, maxZ);
    const float x1 = std::max(minX, maxX);
    const float y1 = std::max(minY, maxY);
    const float z1 = std::max(minZ, maxZ);

    const int rx = std::clamp(resX, 1, 256);
    const int ry = std::clamp(resY, 1, 256);
    const int rz = std::clamp(resZ, 1, 256);

    auto dense = std::make_unique<Phantom::Volume::Volumef>(
        Phantom::Math::Box3df(
            Phantom::Math::Vector3df(x0, y0, z0),
            Phantom::Math::Vector3df(x1, y1, z1)),
        std::array<size_t, 3>{(size_t)rx, (size_t)ry, (size_t)rz});

    for (int i = 0; i < rx; ++i)
        for (int j = 0; j < ry; ++j)
            for (int k = 0; k < rz; ++k)
                dense->setValue({i, j, k}, 0.0f);

    const std::string name = "DenseBox_" + std::to_string(opCount_++);
    auto* scene = world_->addDenseScene(name);
    scene->setVolume(std::move(dense));
    if (pActiveDenseSceneId_) *pActiveDenseSceneId_ = scene->getId();
    if (denseRenderer_) denseRenderer_->markDirty();
    if (onRebuild_) onRebuild_();
    return "OK:" + name;
}

std::string CommandDispatcher::cmdDenseFromSparse(float voxelSize) {
    if (!world_ || !pActiveSceneId_) return "Error:not initialized";

    const auto* src = world_->findById(*pActiveSceneId_);
    if (!src || !src->getShape()) return "Error:no active sparse scene";

    const auto dense = sparseToVolumeWithVoxelSize(*src->getShape(), voxelSize);
    if (dense.getResolutions()[0] == 0) return "Error:empty sparse volume";

    const std::string name = "FromSV_" + src->getName() + "_" + std::to_string(opCount_++);
    auto* scene = world_->addDenseScene(name);
    scene->setVolume(std::make_unique<Phantom::Volume::Volumef>(dense));
    if (pActiveDenseSceneId_) *pActiveDenseSceneId_ = scene->getId();
    if (denseRenderer_) denseRenderer_->markDirty();
    if (onRebuild_) onRebuild_();
    return "OK:" + name;
}

std::string CommandDispatcher::cmdDenseMarchingCubes(float isoLevel) {
    if (!world_ || !pActiveDenseSceneId_) return "Error:not initialized";

    const auto* src = world_->findDenseById(*pActiveDenseSceneId_);
    if (!src || !src->getVolume()) return "Error:no active dense scene";

    Phantom::Volume::MCSurfaceBuilder builder;
    builder.build(*src->getVolume(), isoLevel);
    const auto& tris = builder.getTriangles();

    PolygonMesh mesh;
    mesh.name = "DMC_" + src->getName();
    mesh.positions.reserve(tris.size() * 9);
    mesh.colors.reserve(tris.size() * 12);

    uint32_t idx = 0;
    for (const auto& tri : tris) {
        const auto& verts = tri.getVertices();
        for (int vi = 0; vi < 3; ++vi) {
            mesh.positions.push_back(static_cast<float>(verts[vi].x));
            mesh.positions.push_back(static_cast<float>(verts[vi].y));
            mesh.positions.push_back(static_cast<float>(verts[vi].z));
            mesh.colors.push_back(0.7f);
            mesh.colors.push_back(0.9f);
            mesh.colors.push_back(0.8f);
            mesh.colors.push_back(0.85f);
            mesh.indices.push_back(idx++);
        }
    }

    world_->clearPolygons();
    world_->addPolygon(std::move(mesh));
    if (onRebuild_) onRebuild_();
    return "OK:" + std::to_string(tris.size()) + "tri";
}

std::string CommandDispatcher::cmdDeleteDense(int id) {
    if (!world_) return "Error:no world";

    world_->removeDenseScene(id);
    if (pActiveDenseSceneId_ && *pActiveDenseSceneId_ == id) {
        *pActiveDenseSceneId_ = -1;
        if (!world_->getDenseScenes().empty())
            *pActiveDenseSceneId_ = world_->getDenseScenes().front()->getId();
    }
    if (denseRenderer_) denseRenderer_->markDirty();
    if (onRebuild_) onRebuild_();
    return "OK";
}

} // namespace VolumeView
