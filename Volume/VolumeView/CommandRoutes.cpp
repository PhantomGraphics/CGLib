#include "CommandDispatcher.h"
#include "CommandParse.h"

#include "../VolumeRenderer/PBVRRenderer.h"
#include "../../VkAppBase/VkAppBase.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

// Command routing. route() looks a command up in three tables, in this order:
//   1. exact name, no argument         (queries / actions, e.g. "GetSceneCount")
//   2. "Name:" prefix, rest-of-string  (paths and pixel coordinates, which may themselves contain ':')
//   3. name + field count              (the command split on ':', e.g. "CreateSphere" with 6 fields)
// with the PBVR commands (CommandPbvr.cpp) tried between 2 and 3, and "Error:unknown command" last.
// A name that matches with the wrong field count is "unknown command", which is also what
// CheckCommandCatalog relies on to detect unrouted catalog entries.

namespace VolumeView {

namespace {

struct ExactRoute  { std::string_view name;   std::string (*fn)(CommandDispatcher&); };
struct PrefixRoute { std::string_view prefix; std::string (*fn)(CommandDispatcher&, const std::string& rest); };
struct FieldRoute {
    std::string_view name;
    int              fields;   // parts.size(), or kAnyFields when the handler checks it itself
    std::string (*fn)(CommandDispatcher&, const std::vector<std::string>& parts);
};
constexpr int kAnyFields = -1;

using Parts = std::vector<std::string>;

} // anonymous namespace

std::string CommandDispatcher::route(const std::string& cmd) {
    // The lambdas below are defined inside this member function, so they can use the dispatcher's
    // private state and handlers; being non-capturing they convert to the plain function pointers
    // stored in the tables.
    static const ExactRoute kExact[] = {
        {"CheckCommandCatalog", [](CommandDispatcher& d) { return d.cmdCheckCommandCatalog(); }},
        {"GetSceneCount", [](CommandDispatcher& d) -> std::string {
            if (!d.world_) return "Error:no world";
            return "SceneCount:" + std::to_string(d.world_->getScenes().size());
        }},
        {"GetDenseSceneCount", [](CommandDispatcher& d) -> std::string {
            if (!d.world_) return "Error:no world";
            return "DenseSceneCount:" + std::to_string(d.world_->getDenseScenes().size());
        }},
        {"GetTotalVoxelCount", [](CommandDispatcher& d) -> std::string {
            if (!d.world_) return "Error:no world";
            int total = 0;
            for (const auto& s : d.world_->getScenes())
                if (s->getShape()) total += s->getShape()->getActiveVoxelCount();
            return "TotalVoxelCount:" + std::to_string(total);
        }},
        {"GetVoxelCount", [](CommandDispatcher& d) -> std::string {
            if (!d.world_ || !d.pActiveSceneId_) return "Error:not initialized";
            const auto* scene = d.world_->findById(*d.pActiveSceneId_);
            if (!scene || !scene->getShape()) return "VoxelCount:0";
            return "VoxelCount:" + std::to_string(scene->getShape()->getActiveVoxelCount());
        }},
        {"GetSceneName", [](CommandDispatcher& d) -> std::string {
            if (!d.world_ || !d.pActiveSceneId_) return "Error:not initialized";
            const auto* scene = d.world_->findById(*d.pActiveSceneId_);
            if (!scene) return "Error:no active scene";
            return "SceneName:" + scene->getName();
        }},
        {"ClearWorld", [](CommandDispatcher& d) -> std::string {
            if (!d.world_) return "Error:no world";
            d.world_->clear();
            if (d.pActiveSceneId_) *d.pActiveSceneId_ = -1;
            if (d.pActiveDenseSceneId_) *d.pActiveDenseSceneId_ = -1;
            if (d.onRebuild_) d.onRebuild_();
            return "OK";
        }},
        {"GetPolygonCount", [](CommandDispatcher& d) -> std::string {
            if (!d.world_) return "Error:no world";
            return "PolygonCount:" + std::to_string(d.world_->getPolygons().size());
        }},
        {"ResetCamera", [](CommandDispatcher& d) -> std::string {
            if (d.onCameraReset_) d.onCameraReset_();
            return "OK";
        }},
        {"GetCameraDistance", [](CommandDispatcher& d) -> std::string {
            if (!d.pointRenderer_) return "Error:no renderer";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%f", d.pointRenderer_->getCameraState().distance);
            return buf;
        }},
        {"GetPBVRRepeatCount", [](CommandDispatcher& d) -> std::string {
            if (!d.pbvrRenderer_) return "Error:no pbvr renderer";
            return "RepeatCount:" + std::to_string(d.pbvrRenderer_->getRepeatCount());
        }},
        {"GetVoxelPointSize", [](CommandDispatcher& d) -> std::string {
            if (!d.pointRenderer_) return "Error:no renderer";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "PointSize:%g", d.pointRenderer_->getPointSize());
            return buf;
        }},
        {"GetDensePointSize", [](CommandDispatcher& d) -> std::string {
            if (!d.denseRenderer_) return "Error:no renderer";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "PointSize:%g", d.denseRenderer_->getPointSize());
            return buf;
        }},
        {"GetPBVRShadowEnabled", [](CommandDispatcher& d) -> std::string {
            if (!d.pbvrRenderer_) return "Error:no pbvr renderer";
            return std::string("ShadowEnabled:") + (d.pbvrRenderer_->isShadowEnabled() ? "1" : "0");
        }},
    };

    // Pixel readback answers on a later frame (see processQueue()). Path arguments may contain
    // ':' (drive letters), so these take everything after the prefix.
    static const PrefixRoute kPrefix[] = {
        {"GetPixelColor:", [](CommandDispatcher& d, const std::string& rest) -> std::string {
            uint32_t x, y;
            if (const char* err = parsePixelXY(rest, x, y)) return err;
            return d.cmdGetPixelColor(x, y);
        }},
        {"GetPixelBrightness:", [](CommandDispatcher& d, const std::string& rest) -> std::string {
            uint32_t x, y;
            if (const char* err = parsePixelXY(rest, x, y)) return err;
            return d.cmdGetPixelBrightness(x, y);
        }},
        {"Screenshot:", [](CommandDispatcher& d, const std::string& path) -> std::string {
            if (!d.app_) return "Error:no app";
            d.app_->requestScreenshot(path);
            return "OK:" + path;
        }},
        {"SetShowVolumeGrid:", [](CommandDispatcher& d, const std::string& val) -> std::string {
            if (!d.menuPanel_) return "Error:no menu panel";
            d.menuPanel_->setShowVolumeGrid(val != "0");
            return "OK";
        }},
        {"SetShowVectorField:", [](CommandDispatcher& d, const std::string& val) -> std::string {
            if (!d.menuPanel_) return "Error:no menu panel";
            d.menuPanel_->setShowVectorField(val != "0");
            return "OK";
        }},
        {"DumpPBVRParticleRadiance:", [](CommandDispatcher& d, const std::string& path) -> std::string {
            if (!d.pbvrRenderer_) return "Error:no pbvr renderer";
            return d.pbvrRenderer_->dumpParticleRadiance(path) ? "OK" : "Error:no scattering result to dump";
        }},
    };

    static const FieldRoute kFields[] = {
        {"SetActiveScene", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            if (!d.world_ || !d.pActiveSceneId_) return "Error:not initialized";
            int idx = 0;
            if (!parseInt(p[1], idx)) return "Error:bad index";
            const auto& scenes = d.world_->getScenes();
            if (idx < 0 || idx >= static_cast<int>(scenes.size())) return "Error:index out of range";
            *d.pActiveSceneId_ = scenes[static_cast<size_t>(idx)]->getId();
            return "OK";
        }},
        {"SetActiveDenseScene", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            if (!d.world_ || !d.pActiveDenseSceneId_) return "Error:not initialized";
            int idx = 0;
            if (!parseInt(p[1], idx)) return "Error:bad index";
            const auto& scenes = d.world_->getDenseScenes();
            if (idx < 0 || idx >= static_cast<int>(scenes.size())) return "Error:index out of range";
            *d.pActiveDenseSceneId_ = scenes[static_cast<size_t>(idx)]->getId();
            return "OK";
        }},
        {"CreateSphere", 6, [](CommandDispatcher& d, const Parts& p) -> std::string {
            float f[5];
            if (!parseFloats(p, 1, 5, f)) return "Error:bad CreateSphere params";
            return d.cmdCreateSphere(f[0], f[1], f[2], f[3], f[4]);
        }},
        {"CreateBox", 8, [](CommandDispatcher& d, const Parts& p) -> std::string {
            float f[7];
            if (!parseFloats(p, 1, 7, f)) return "Error:bad CreateBox params";
            return d.cmdCreateBox(f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
        }},
        {"CSGCombine", 4, [](CommandDispatcher& d, const Parts& p) -> std::string {
            int idxA, idxB;
            if (!parseInt(p[2], idxA) || !parseInt(p[3], idxB)) return "Error:bad CSGCombine indices";
            return d.cmdCsgCombine(p[1], idxA, idxB);
        }},
        {"Resample", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            float newCell;
            if (!parseFloat(p[1], newCell)) return "Error:bad Resample cell size";
            return d.cmdResample(newCell);
        }},
        {"MarchingCubes", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            float isoLevel;
            if (!parseFloat(p[1], isoLevel)) return "Error:bad MarchingCubes isoLevel";
            return d.cmdMarchingCubes(isoLevel);
        }},
        {"CreateDenseBox", 10, [](CommandDispatcher& d, const Parts& p) -> std::string {
            float f[6];
            int   n[3];
            if (!parseFloats(p, 1, 6, f) || !parseInts(p, 7, 3, n)) return "Error:bad CreateDenseBox params";
            return d.cmdCreateDenseBox(f[0], f[1], f[2], f[3], f[4], f[5], n[0], n[1], n[2]);
        }},
        {"DenseFromSparse", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            float voxelSize;
            if (!parseFloat(p[1], voxelSize)) return "Error:bad DenseFromSparse voxelSize";
            return d.cmdDenseFromSparse(voxelSize);
        }},
        {"DenseMarchingCubes", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            float isoLevel;
            if (!parseFloat(p[1], isoLevel)) return "Error:bad DenseMarchingCubes isoLevel";
            return d.cmdDenseMarchingCubes(isoLevel);
        }},
        {"DeleteDense", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            int id = -1;
            if (!parseInt(p[1], id)) return "Error:bad DeleteDense id";
            return d.cmdDeleteDense(id);
        }},
        {"SetPBVRRenderMode", 2, [](CommandDispatcher& d, const Parts& p) -> std::string {
            if (!d.menuPanel_) return "Error:no menu panel";
            int mode = 0;
            if (!parseInt(p[1], mode) || mode < 0 || mode > 2) return "Error:bad SetPBVRRenderMode value";
            d.menuPanel_->setRenderMode(mode);
            return "OK";
        }},
        // The menu setters check their own field count, so any count reaches the handler.
        {"SetVoxelPointSize", kAnyFields, [](CommandDispatcher& d, const Parts& p) -> std::string {
            return d.setMenuValue(p, [](MenuPanel& m, float f, const std::string&) { m.setVoxelPointSize(f); return true; });
        }},
        {"SetDensePointSize", kAnyFields, [](CommandDispatcher& d, const Parts& p) -> std::string {
            return d.setMenuValue(p, [](MenuPanel& m, float f, const std::string&) { m.setDensePointSize(f); return true; });
        }},
        {"SetDensePoints", kAnyFields, [](CommandDispatcher& d, const Parts& p) -> std::string {
            return d.setMenuValue(p, [](MenuPanel& m, float, const std::string& raw) { m.setShowDensePoints(raw != "0"); return true; });
        }},
        {"SetDenseColorMap", kAnyFields, [](CommandDispatcher& d, const Parts& p) -> std::string {
            return d.setMenuValue(p, [](MenuPanel& m, float f, const std::string&) {
                if (f < 0.0f || f > 2.0f) return false;
                m.setDenseColorMap(static_cast<int>(f));
                return true;
            });
        }},
    };

    for (const auto& r : kExact)
        if (cmd == r.name) return r.fn(*this);

    for (const auto& r : kPrefix)
        if (cmd.rfind(r.prefix, 0) == 0) return r.fn(*this, cmd.substr(r.prefix.size()));

    const auto parts = splitBy(cmd, ':');
    if (parts.empty()) return "Error:empty command";

    if (auto r = routePbvr(cmd, parts)) return *r;

    for (const auto& r : kFields)
        if (parts[0] == r.name && (r.fields == kAnyFields || static_cast<int>(parts.size()) == r.fields))
            return r.fn(*this, parts);

    return "Error:unknown command '" + cmd + "'";
}

// "x,y" -> non-negative pixel coordinates; returns an error message, or nullptr on success.
const char* CommandDispatcher::parsePixelXY(const std::string& text, uint32_t& x, uint32_t& y) {
    const auto p = splitBy(text, ',');
    if (p.size() != 2) return "Error:expected x,y";
    int ix, iy;
    if (!parseInt(p[0], ix) || !parseInt(p[1], iy) || ix < 0 || iy < 0) return "Error:invalid coordinates";
    x = static_cast<uint32_t>(ix);
    y = static_cast<uint32_t>(iy);
    return nullptr;
}

// Shared body of the menu setters: "Name:<float>" with exactly two fields. `apply` returns false
// when the value is rejected ("Error:bad <Name> value").
std::string CommandDispatcher::setMenuValue(const std::vector<std::string>& parts, MenuApply apply) {
    if (parts.size() != 2) return "Error:bad " + parts[0] + " value";
    if (!menuPanel_) return "Error:no menu panel";
    float f = 0.0f;
    if (!parseFloat(parts[1], f)) return "Error:bad " + parts[0] + " value";
    if (!apply(*menuPanel_, f, parts[1])) return "Error:bad " + parts[0] + " value";
    return "OK";
}

} // namespace VolumeView
