#include "CommandParse.h"

#include <charconv>
#include <cstring>

namespace VolumeView {

namespace {

template <typename T>
bool parseNumber(const std::string& s, T& out) {
    T value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    // Same leniency as the dispatcher always had: only the leading number must parse, trailing
    // text is ignored ("12abc" -> 12). Tightening that is a behaviour change for its own commit.
    (void)ptr;
    if (ec != std::errc{}) return false;
    out = value;
    return true;
}

} // namespace

bool parseFloat(const std::string& s, float& out) { return parseNumber(s, out); }
bool parseInt(const std::string& s, int& out)     { return parseNumber(s, out); }

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

bool parseFloats(const std::vector<std::string>& parts, size_t first, size_t count, float* out) {
    if (first + count > parts.size()) return false;
    for (size_t i = 0; i < count; ++i)
        if (!parseFloat(parts[first + i], out[i])) return false;
    return true;
}

bool parseInts(const std::vector<std::string>& parts, size_t first, size_t count, int* out) {
    if (first + count > parts.size()) return false;
    for (size_t i = 0; i < count; ++i)
        if (!parseInt(parts[first + i], out[i])) return false;
    return true;
}

const std::vector<ScalarCommandSpec>& pbvrScalarCommands() {
    static const std::vector<ScalarCommandSpec> kCommands = {
        {"SetPBVRUseGPU",               ScalarArg::Flag},
        {"SetPBVRParticleSize",         ScalarArg::Float},
        {"SetPBVRDensityScale",         ScalarArg::Float},
        {"SetPBVRCameraDistance",       ScalarArg::Float},
        {"SetPBVRMultipleScattering",   ScalarArg::Flag},
        {"SetPBVRScatteringOrders",     ScalarArg::Int},
        {"SetPBVRProbeCount",           ScalarArg::Int},
        {"SetPBVRProbeRadius",          ScalarArg::Float},
        {"SetPBVRPhaseG",               ScalarArg::Float},
        {"SetPBVRScatteringAlbedo",     ScalarArg::Float},
        {"SetPBVRCameraFov",            ScalarArg::Float},
        {"SetPBVRProbeSourceBudget",    ScalarArg::Int},
        {"SetPBVRScatteringSHDegree",   ScalarArg::Int},
        {"SetPBVRScatteringExposure",   ScalarArg::Float},
        {"SetPBVRTFPreset",             ScalarArg::Int},
        {"SetPBVRShadowEnabled",        ScalarArg::Flag},
        {"SetPBVRExtinction",           ScalarArg::Float},
        {"SetPBVRShadowLayers",         ScalarArg::Int},
        {"SetPBVRRepeatCount",          ScalarArg::Int},
        {"SetPBVRMaxParticlesPerVoxel", ScalarArg::Int},
    };
    return kCommands;
}

const ScalarCommandSpec* findScalarCommand(const std::vector<std::string>& parts) {
    if (parts.size() != 2) return nullptr;
    for (const auto& spec : pbvrScalarCommands())
        if (parts[0] == spec.name) return &spec;
    return nullptr;
}

bool parseScalarValue(const ScalarCommandSpec& spec, const std::string& text, ScalarValue& out) {
    switch (spec.kind) {
        case ScalarArg::Float: return parseFloat(text, out.f);
        case ScalarArg::Int:   return parseInt(text, out.i);
        case ScalarArg::Flag:  out.flag = (text != "0"); return true;
    }
    return false;
}

} // namespace VolumeView
