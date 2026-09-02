#include "CommandDispatcher.h"

#include "VolumeScene.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Coord.h"
#include "../Volume/SparseVolumeTree/Interpolator.h"
#include "../Volume/LevelSet.h"
#include "../Volume/MCSurfaceBuilder.h"
#include "../Volume/Volume.h"
#include "../VolumeRenderer/PBVRRenderer.h"
#include "../../VkAppBase/VkAppBase.h"
#include "../Volume/SparseVolumeTree/VdbReader.h"
#include "../Volume/SparseVolumeTree/VdbWriter.h"

#define GLM_FORCE_RADIANS
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace VkVolumeView {

namespace {

bool parseFloat(const std::string& s, float& out) {
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{};
}

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

bool parseInt(const std::string& s, int& out) {
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{};
}

std::vector<std::string> splitBy(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string part;
    for (char c : s) {
        if (c == delim) { parts.push_back(std::move(part)); part.clear(); }
        else             { part += c; }
    }
    parts.push_back(std::move(part));
    return parts;
}

} // anonymous namespace

// ============================================================
//  IScenarioDispatcher interface
// ============================================================

void CommandDispatcher::dispatch(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    inputQueue_.push(command);
}

std::vector<std::string> CommandDispatcher::collectResponses() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    while (!outputQueue_.empty()) {
        out.push_back(std::move(outputQueue_.front()));
        outputQueue_.pop();
    }
    return out;
}

void CommandDispatcher::processQueue() {
    // If a pixel read is pending, check whether the result is available before processing
    // any new commands from the input queue.
    if (pixelReadPending_) {
        uint8_t data[4];
        if (app_ && app_->pollPixelRead(data)) {
            std::string resp;
            if (pixelBrightness_) {
                char buf[32];
                const float lum = (data[0] * 0.299f + data[1] * 0.587f + data[2] * 0.114f) / 255.f;
                std::snprintf(buf, sizeof(buf), "%.4f", lum);
                resp = buf;
            } else {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "0x%02X%02X%02X",
                    static_cast<unsigned>(data[0]), static_cast<unsigned>(data[1]), static_cast<unsigned>(data[2]));
                resp = buf;
            }
            std::lock_guard<std::mutex> lock(mutex_);
            outputQueue_.push(std::move(resp));
            pixelReadPending_ = false;
            pixelBrightness_  = false;
        }
        return; // Don't consume new commands until the read completes.
    }

    std::queue<std::string> local;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(local, inputQueue_);
    }
    while (!local.empty()) {
        std::string cmd = std::move(local.front());
        local.pop();
        std::string resp = route(cmd);

        if (pixelReadPending_) {
            // GetPixelColor/GetPixelBrightness was just dispatched; re-queue any remaining
            // commands so they are processed only after the pixel read resolves (see the
            // pixelReadPending_ branch above), keeping responses in 1:1 order with commands.
            std::lock_guard<std::mutex> lock(mutex_);
            while (!local.empty()) {
                inputQueue_.push(std::move(local.front()));
                local.pop();
            }
            break;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            outputQueue_.push(std::move(resp));
        }
    }
}

// ============================================================
//  Command routing
// ============================================================

std::string CommandDispatcher::route(const std::string& cmd) {
    // --- Simple query / action commands ---

    if (cmd == "GetSceneCount") {
        if (!world_) return "Error:no world";
        return "SceneCount:" + std::to_string(world_->getScenes().size());
    }

    if (cmd == "GetDenseSceneCount") {
        if (!world_) return "Error:no world";
        return "DenseSceneCount:" + std::to_string(world_->getDenseScenes().size());
    }

    if (cmd == "GetTotalVoxelCount") {
        if (!world_) return "Error:no world";
        int total = 0;
        for (const auto& s : world_->getScenes())
            if (s->getShape()) total += s->getShape()->getActiveVoxelCount();
        return "TotalVoxelCount:" + std::to_string(total);
    }

    if (cmd == "GetVoxelCount") {
        if (!world_ || !pActiveSceneId_) return "Error:not initialized";
        const auto* scene = world_->findById(*pActiveSceneId_);
        if (!scene || !scene->getShape()) return "VoxelCount:0";
        return "VoxelCount:" + std::to_string(scene->getShape()->getActiveVoxelCount());
    }

    if (cmd == "GetSceneName") {
        if (!world_ || !pActiveSceneId_) return "Error:not initialized";
        const auto* scene = world_->findById(*pActiveSceneId_);
        if (!scene) return "Error:no active scene";
        return "SceneName:" + scene->getName();
    }

    if (cmd == "ClearWorld") {
        if (!world_) return "Error:no world";
        world_->clear();
        if (pActiveSceneId_) *pActiveSceneId_ = -1;
        if (pActiveDenseSceneId_) *pActiveDenseSceneId_ = -1;
        if (onRebuild_) onRebuild_();
        return "OK";
    }

    if (cmd == "GetPolygonCount") {
        if (!world_) return "Error:no world";
        return "PolygonCount:" + std::to_string(world_->getPolygons().size());
    }

    if (cmd == "ResetCamera") {
        if (onCameraReset_) onCameraReset_();
        return "OK";
    }

    if (cmd == "GetCameraDistance") {
        if (!pointRenderer_) return "Error:no renderer";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%f", pointRenderer_->getCameraState().distance);
        return buf;
    }

    if (cmd == "GetPBVRShadowEnabled") {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return std::string("ShadowEnabled:") + (pbvrRenderer_->isShadowEnabled() ? "1" : "0");
    }

    // --- Pixel readback: response is deferred to the frame after the swap-chain copy ---

    if (cmd.rfind("GetPixelColor:", 0) == 0 || cmd.rfind("GetPixelBrightness:", 0) == 0) {
        const bool brightness = cmd.rfind("GetPixelBrightness:", 0) == 0;
        const std::string rest = cmd.substr(cmd.find(':') + 1);
        const auto p = splitBy(rest, ',');
        if (p.size() != 2) return "Error:expected x,y";
        int x, y;
        if (!parseInt(p[0], x) || !parseInt(p[1], y) || x < 0 || y < 0)
            return "Error:invalid coordinates";
        return brightness ? cmdGetPixelBrightness(static_cast<uint32_t>(x), static_cast<uint32_t>(y))
                           : cmdGetPixelColor(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    }

    // --- Path-based commands: extract everything after the first colon ---

    static constexpr std::string_view kScreenshot = "Screenshot:";
    if (cmd.rfind(kScreenshot, 0) == 0) {
        if (!app_) return "Error:no app";
        const std::string path = cmd.substr(kScreenshot.size());
        app_->requestScreenshot(path);
        return "OK:" + path;
    }

    static constexpr std::string_view kSetShowVolumeGrid = "SetShowVolumeGrid:";
    if (cmd.rfind(kSetShowVolumeGrid, 0) == 0) {
        if (!menuPanel_) return "Error:no menu panel";
        const std::string val = cmd.substr(kSetShowVolumeGrid.size());
        menuPanel_->setShowVolumeGrid(val != "0");
        return "OK";
    }

    static constexpr std::string_view kSetShowVectorField = "SetShowVectorField:";
    if (cmd.rfind(kSetShowVectorField, 0) == 0) {
        if (!menuPanel_) return "Error:no menu panel";
        const std::string val = cmd.substr(kSetShowVectorField.size());
        menuPanel_->setShowVectorField(val != "0");
        return "OK";
    }

    static constexpr std::string_view kExportVDB = "ExportVDB:";
    if (cmd.rfind(kExportVDB, 0) == 0) {
        return cmdExportVDB(cmd.substr(kExportVDB.size()));
    }

    static constexpr std::string_view kImportVDB = "ImportVDB:";
    if (cmd.rfind(kImportVDB, 0) == 0) {
        return cmdImportVDB(cmd.substr(kImportVDB.size()));
    }

    // --- Parametric commands: split entire string by ':' ---

    const auto parts = splitBy(cmd, ':');
    if (parts.empty()) return "Error:empty command";

    if (parts[0] == "SetActiveScene" && parts.size() == 2) {
        if (!world_ || !pActiveSceneId_) return "Error:not initialized";
        int idx = 0;
        if (!parseInt(parts[1], idx)) return "Error:bad index";
        const auto& scenes = world_->getScenes();
        if (idx < 0 || idx >= static_cast<int>(scenes.size()))
            return "Error:index out of range";
        *pActiveSceneId_ = scenes[static_cast<size_t>(idx)]->getId();
        return "OK";
    }

    if (parts[0] == "SetActiveDenseScene" && parts.size() == 2) {
        if (!world_ || !pActiveDenseSceneId_) return "Error:not initialized";
        int idx = 0;
        if (!parseInt(parts[1], idx)) return "Error:bad index";
        const auto& scenes = world_->getDenseScenes();
        if (idx < 0 || idx >= static_cast<int>(scenes.size()))
            return "Error:index out of range";
        *pActiveDenseSceneId_ = scenes[static_cast<size_t>(idx)]->getId();
        return "OK";
    }

    if (parts[0] == "CreateSphere" && parts.size() == 6) {
        float cx, cy, cz, radius, cell;
        if (!parseFloat(parts[1], cx)     || !parseFloat(parts[2], cy) ||
            !parseFloat(parts[3], cz)     || !parseFloat(parts[4], radius) ||
            !parseFloat(parts[5], cell))
            return "Error:bad CreateSphere params";
        return cmdCreateSphere(cx, cy, cz, radius, cell);
    }

    if (parts[0] == "CreateBox" && parts.size() == 8) {
        float minX, minY, minZ, maxX, maxY, maxZ, cell;
        if (!parseFloat(parts[1], minX) || !parseFloat(parts[2], minY) ||
            !parseFloat(parts[3], minZ) || !parseFloat(parts[4], maxX) ||
            !parseFloat(parts[5], maxY) || !parseFloat(parts[6], maxZ) ||
            !parseFloat(parts[7], cell))
            return "Error:bad CreateBox params";
        return cmdCreateBox(minX, minY, minZ, maxX, maxY, maxZ, cell);
    }

    if (parts[0] == "CSGCombine" && parts.size() == 4) {
        int idxA, idxB;
        if (!parseInt(parts[2], idxA) || !parseInt(parts[3], idxB))
            return "Error:bad CSGCombine indices";
        return cmdCsgCombine(parts[1], idxA, idxB);
    }

    if (parts[0] == "Resample" && parts.size() == 2) {
        float newCell;
        if (!parseFloat(parts[1], newCell)) return "Error:bad Resample cell size";
        return cmdResample(newCell);
    }

    if (parts[0] == "MarchingCubes" && parts.size() == 2) {
        float isoLevel;
        if (!parseFloat(parts[1], isoLevel)) return "Error:bad MarchingCubes isoLevel";
        return cmdMarchingCubes(isoLevel);
    }

    if (parts[0] == "CreateDenseBox" && parts.size() == 10) {
        float minX, minY, minZ, maxX, maxY, maxZ;
        int resX, resY, resZ;
        if (!parseFloat(parts[1], minX) || !parseFloat(parts[2], minY) ||
            !parseFloat(parts[3], minZ) || !parseFloat(parts[4], maxX) ||
            !parseFloat(parts[5], maxY) || !parseFloat(parts[6], maxZ) ||
            !parseInt(parts[7], resX) || !parseInt(parts[8], resY) || !parseInt(parts[9], resZ))
            return "Error:bad CreateDenseBox params";
        return cmdCreateDenseBox(minX, minY, minZ, maxX, maxY, maxZ, resX, resY, resZ);
    }

    if (parts[0] == "DenseFromSparse" && parts.size() == 2) {
        float voxelSize;
        if (!parseFloat(parts[1], voxelSize)) return "Error:bad DenseFromSparse voxelSize";
        return cmdDenseFromSparse(voxelSize);
    }

    if (parts[0] == "DenseMarchingCubes" && parts.size() == 2) {
        float isoLevel;
        if (!parseFloat(parts[1], isoLevel)) return "Error:bad DenseMarchingCubes isoLevel";
        return cmdDenseMarchingCubes(isoLevel);
    }

    if (parts[0] == "DeleteDense" && parts.size() == 2) {
        int id = -1;
        if (!parseInt(parts[1], id)) return "Error:bad DeleteDense id";
        return cmdDeleteDense(id);
    }

    // --- PBVR self-shadow (experimental, see internal design notes) ---

    if (parts[0] == "SetPBVRRenderMode" && parts.size() == 2) {
        if (!menuPanel_) return "Error:no menu panel";
        int mode = 0;
        if (!parseInt(parts[1], mode) || mode < 0 || mode > 2) return "Error:bad SetPBVRRenderMode value";
        menuPanel_->setRenderMode(mode);
        return "OK";
    }

    if (parts[0] == "SetPBVRUseGPU" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        pbvrRenderer_->setUseGPU(parts[1] != "0");
        return "OK";
    }

    if (parts[0] == "SetPBVRParticleSize" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        float size;
        if (!parseFloat(parts[1], size)) return "Error:bad SetPBVRParticleSize value";
        pbvrRenderer_->setParticleSize(size);
        return "OK";
    }

    if (parts[0] == "SetPBVRDensityScale" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        float scale;
        if (!parseFloat(parts[1], scale)) return "Error:bad SetPBVRDensityScale value";
        pbvrRenderer_->setDensityScale(scale);
        return "OK";
    }

    if (parts[0] == "SetPBVRTFPreset" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        int preset = 0;
        if (!parseInt(parts[1], preset)) return "Error:bad SetPBVRTFPreset value";
        pbvrRenderer_->setTransferFunctionPreset(preset);
        return "OK";
    }

    if (parts[0] == "SetPBVRShadowEnabled" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        pbvrRenderer_->setShadowEnabled(parts[1] != "0");
        return "OK";
    }

    if (parts[0] == "SetPBVRLightDir" && parts.size() == 3) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        float az, el;
        if (!parseFloat(parts[1], az) || !parseFloat(parts[2], el)) return "Error:bad SetPBVRLightDir params";
        pbvrRenderer_->setLightDir(az, el);
        return "OK";
    }

    if (parts[0] == "SetPBVRExtinction" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        float sigma;
        if (!parseFloat(parts[1], sigma)) return "Error:bad SetPBVRExtinction value";
        pbvrRenderer_->setExtinction(sigma);
        return "OK";
    }

    if (parts[0] == "SetPBVRShadowLayers" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        int n;
        if (!parseInt(parts[1], n)) return "Error:bad SetPBVRShadowLayers value";
        pbvrRenderer_->setShadowLayers(n);
        return "OK";
    }

    if (parts[0] == "SetPBVRShadowMapSize" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        int size;
        if (!parseInt(parts[1], size) || size <= 0) return "Error:bad SetPBVRShadowMapSize value";
        pbvrRenderer_->setShadowMapSize(static_cast<uint32_t>(size));
        return "OK";
    }

    return "Error:unknown command '" + cmd + "'";
}

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

// ============================================================
//  Helper command implementations
// ============================================================

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

std::string CommandDispatcher::cmdGetPixelColor(uint32_t x, uint32_t y) {
    if (!app_) return "Error:app not available";
    app_->requestPixelRead(x, y);
    pixelReadPending_ = true;
    pixelBrightness_  = false;
    return {}; // response is deferred to the next frame
}

std::string CommandDispatcher::cmdGetPixelBrightness(uint32_t x, uint32_t y) {
    if (!app_) return "Error:app not available";
    app_->requestPixelRead(x, y);
    pixelReadPending_ = true;
    pixelBrightness_  = true;
    return {}; // response is deferred to the next frame
}

std::string CommandDispatcher::cmdExportVDB(const std::string& path) {
    if (!world_ || !pActiveSceneId_) return "Error:not initialized";

    const auto* src = world_->findById(*pActiveSceneId_);
    if (!src || !src->getShape()) return "Error:no active sparse scene";

    Phantom::Volume::SparseVolumeVdbWriter writer;
    if (!writer.write(path, *src->getShape(), "density")) return "Error:VDB write failed";
    return "OK";
}

std::string CommandDispatcher::cmdImportVDB(const std::string& path) {
    if (!world_) return "Error:no world";

    Phantom::Volume::SparseVolumeVdbReader reader;
    auto sparse = reader.read(path);
    if (!sparse) return "Error:VDB read failed";

    const std::string name = "Imported_" + std::to_string(opCount_++);
    auto* scene = world_->addScene(name);
    const int voxels = sparse->getActiveVoxelCount();
    scene->setShape(std::move(sparse));
    if (pActiveSceneId_) *pActiveSceneId_ = scene->getId();
    if (onRebuild_) onRebuild_();
    return "OK:" + name + ":" + std::to_string(voxels) + "vx";
}

} // namespace VkVolumeView
