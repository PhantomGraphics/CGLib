#include "CommandDispatcher.h"
#include "CommandParse.h"

#include "../VolumeRenderer/PBVRRenderer.h"

#include <glm/vec3.hpp>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// PBVR renderer commands. Name/arity/value kind of the single-value setters live in CommandParse
// (testable without a GPU); only the call into the renderer is here.

namespace VolumeView {

namespace {

// Renderer-side half of the PBVR single-value commands; names/kinds are in pbvrScalarCommands().
using PbvrApply = std::function<void(Phantom::Volume::PBVRRenderer&, const ScalarValue&)>;
const std::unordered_map<std::string, PbvrApply> kPbvrScalarSetters = {
    {"SetPBVRUseGPU",             [](auto& r, const ScalarValue& v) { r.setUseGPU(v.flag); }},
    {"SetPBVRParticleSize",       [](auto& r, const ScalarValue& v) { r.setParticleSize(v.f); }},
    {"SetPBVRDensityScale",       [](auto& r, const ScalarValue& v) { r.setDensityScale(v.f); }},
    {"SetPBVRCameraDistance",     [](auto& r, const ScalarValue& v) { r.setCameraDistance(v.f); }},
    {"SetPBVRMultipleScattering", [](auto& r, const ScalarValue& v) { r.setMultipleScatteringEnabled(v.flag); }},
    {"SetPBVRScatteringOrders",   [](auto& r, const ScalarValue& v) { r.setScatteringOrders(v.i); }},
    {"SetPBVRProbeCount",         [](auto& r, const ScalarValue& v) { r.setProbeCount(v.i); }},
    {"SetPBVRProbeRadius",        [](auto& r, const ScalarValue& v) { r.setProbeRadius(v.f); }},
    {"SetPBVRPhaseG",             [](auto& r, const ScalarValue& v) { r.setPhaseG(v.f); }},
    {"SetPBVRScatteringAlbedo",   [](auto& r, const ScalarValue& v) { r.setScatteringAlbedo(v.f); }},
    {"SetPBVRCameraFov",          [](auto& r, const ScalarValue& v) { r.setCameraFovY(v.f); }},
    {"SetPBVRProbeSourceBudget",  [](auto& r, const ScalarValue& v) { r.setProbeSourceBudget(v.i); }},
    {"SetPBVRScatteringSHDegree", [](auto& r, const ScalarValue& v) { r.setScatteringSHDegree(v.i); }},
    {"SetPBVRScatteringExposure", [](auto& r, const ScalarValue& v) { r.setScatteringExposure(v.f); }},
    {"SetPBVRTFPreset",           [](auto& r, const ScalarValue& v) { r.setTransferFunctionPreset(v.i); }},
    {"SetPBVRShadowEnabled",      [](auto& r, const ScalarValue& v) { r.setShadowEnabled(v.flag); }},
    {"SetPBVRExtinction",         [](auto& r, const ScalarValue& v) { r.setExtinction(v.f); }},
    {"SetPBVRShadowLayers",       [](auto& r, const ScalarValue& v) { r.setShadowLayers(v.i); }},
    {"SetPBVRRepeatCount",        [](auto& r, const ScalarValue& v) { r.setRepeatCount(v.i); }},
    {"SetPBVRMaxParticlesPerVoxel", [](auto& r, const ScalarValue& v) { r.setMaxParticlesPerVoxel(v.i); }},
};

} // anonymous namespace

// Returns nullopt when `cmd` is not a PBVR command handled here, so route() keeps looking.
std::optional<std::string> CommandDispatcher::routePbvr(const std::string& cmd,
                                                        const std::vector<std::string>& parts) {
    // --- PBVR single-value setters: name/arity/value kind live in CommandParse (testable without a
    //     GPU); only the call into the renderer is here ---

    if (const ScalarCommandSpec* spec = findScalarCommand(parts)) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        ScalarValue v;
        if (!parseScalarValue(*spec, parts[1], v))
            return std::string("Error:bad ") + spec->name + " value";
        const auto it = kPbvrScalarSetters.find(spec->name);
        if (it == kPbvrScalarSetters.end()) return "Error:unknown command '" + cmd + "'";
        it->second(*pbvrRenderer_, v);
        return "OK";
    }

    if (parts[0] == "GetPBVRMeanScatteredRadiance") {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return std::to_string(pbvrRenderer_->getMeanScatteredRadiance());
    }

    if (parts[0] == "GetPBVRMeanIndirectRadiance") {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return std::to_string(pbvrRenderer_->getMeanIndirectRadiance());
    }

    if (parts[0] == "GetPBVRMeanSunTransmittance") {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return std::to_string(pbvrRenderer_->getMeanSunTransmittance());
    }

    if (parts[0] == "SetPBVRCameraTarget" && parts.size() == 4) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        float v[3];
        if (!parseFloats(parts, 1, 3, v)) return "Error:bad SetPBVRCameraTarget value";
        pbvrRenderer_->setCameraTarget(glm::vec3(v[0], v[1], v[2]));
        return "OK";
    }

    if (parts[0] == "SetPBVRCameraAngles" && parts.size() == 3) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        float a[2];
        if (!parseFloats(parts, 1, 2, a)) return "Error:bad SetPBVRCameraAngles value";
        pbvrRenderer_->setCameraAngles(a[0], a[1]);
        return "OK";
    }

    if (parts[0] == "GetPBVRScatteringSolveMs") {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        return std::to_string(pbvrRenderer_->getLastScatteringSolveMs());
    }

    if (parts[0] == "SetPBVRLightDir" && parts.size() == 3) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        float a[2];
        if (!parseFloats(parts, 1, 2, a)) return "Error:bad SetPBVRLightDir params";
        pbvrRenderer_->setLightDir(a[0], a[1]);
        return "OK";
    }

    if (parts[0] == "SetPBVRShadowMapSize" && parts.size() == 2) {
        if (!pbvrRenderer_) return "Error:no pbvr renderer";
        int size;
        if (!parseInt(parts[1], size) || size <= 0) return "Error:bad SetPBVRShadowMapSize value";
        pbvrRenderer_->setShadowMapSize(static_cast<uint32_t>(size));
        return "OK";
    }

    return std::nullopt;
}

} // namespace VolumeView
