#include "PhmatCompiler.h"

#include "PhmatReflection.h"
#include "json.hpp"

#include "../../../CGLib/VulkanGraphics/VulkanSPVLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace Phantom::Gltf::Phmat
{
namespace {

using Json = nlohmann::json;

// Everything CGLib/GltfViewer/shaders/gltf.frag declares before its own `void main()` --
// set=0 global resources, vertex-stage inputs/outputs, and the Cook-Torrance/shadow helper
// functions -- copied verbatim, MINUS the set=1 MaterialUBO block (binding 0). A phmat-generated
// shader never reads MaterialUBO (the graph supplies baseColor/metallic/roughness/normal/
// occlusion/emissive itself), and Vulkan does not require a shader to statically reference every
// binding in a descriptor set layout it is bound against -- so materialSetLayout_ (still
// declaring binding 0) stays usable unchanged, this text simply never mentions it. The 5 texture
// samplers (bindings 1-5) ARE kept, since textureSlot/normalMap node codegen samples them.
constexpr const char* kGltfPbrHeader = R"GLSL(#version 450

// set=0: global (per-frame)
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  camPos;
    vec4  lightPos;    // w=0: directional, w=1: point
    vec4  lightColor;  // w=intensity
    int   useIBL;
    int   shadowEnabled;
    float shadowBias;
    float shadowStrength;
} cam;
layout(set = 0, binding = 1) uniform samplerCube irradianceMap;
layout(set = 0, binding = 2) uniform samplerCube prefilteredEnvMap;
layout(set = 0, binding = 3) uniform sampler2D   brdfLUT;
layout(set = 0, binding = 4) uniform sampler2D   shadowMap;

// set=1: per-material textures only -- see this file's kGltfPbrHeader comment for why
// MaterialUBO (binding 0) is intentionally not declared here.
layout(set = 1, binding = 1) uniform sampler2D baseColorTex;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTex;
layout(set = 1, binding = 3) uniform sampler2D normalTex;
layout(set = 1, binding = 4) uniform sampler2D occlusionTex;
layout(set = 1, binding = 5) uniform sampler2D emissiveTex;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;
layout(location = 5) in vec4 fragPosLightSpace;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979323846;
const float MAX_REFLECTION_LOD = 4.0;

float distributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// PCF (3x3) shadow lookup. Returns 0 = fully lit, 1 = fully occluded.
float computeShadow(vec4 posLightSpace, float NdotL) {
    vec3 proj = posLightSpace.xyz / posLightSpace.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.z > 1.0 || any(lessThan(proj.xy, vec2(0.0))) || any(greaterThan(proj.xy, vec2(1.0))))
        return 0.0;

    float bias = max(cam.shadowBias * (1.0 - NdotL), cam.shadowBias * 0.1);
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(shadowMap, proj.xy + vec2(x, y) * texel).r;
            shadow += (proj.z - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    return (shadow / 9.0) * cam.shadowStrength;
}
)GLSL";

// From "vec3 V = ..." through the IBL block (inclusive) -- identical to gltf.frag. Occlusion and
// emissive are generated per-graph and spliced in by the caller between this and kLightingTail2.
constexpr const char* kLightingTail1 = R"GLSL(
    vec3 V = normalize(cam.camPos.xyz - fragPos);
    vec3 R = reflect(-V, N);

    vec3 L;
    if (cam.lightPos.w == 0.0) {
        L = normalize(cam.lightPos.xyz);
    } else {
        L = normalize(cam.lightPos.xyz - fragPos);
    }
    vec3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0001);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 albedo = baseColor.rgb;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3  F = fresnelSchlick(VdotH, F0);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float shadow = (cam.shadowEnabled != 0) ? computeShadow(fragPosLightSpace, NdotL) : 0.0;
    vec3 Lo = (kD * albedo / PI + specular) * cam.lightColor.rgb * NdotL * (1.0 - shadow);

    vec3 ambient = vec3(0.03) * albedo;

    if (cam.useIBL == 1) {
        vec3 F_ibl  = fresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - metallic);
        vec3 irradiance      = texture(irradianceMap, N).rgb;
        vec3 diffuseIBL      = kD_ibl * irradiance * albedo;
        vec2 brdf            = texture(brdfLUT, vec2(NdotV, roughness)).rg;
        vec3 prefilteredColor = textureLod(prefilteredEnvMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec3 specularIBL     = prefilteredColor * (F_ibl * brdf.x + brdf.y);
        ambient = diffuseIBL + specularIBL;
    }

)GLSL";

// Final composite + Reinhard tonemap + output -- identical to gltf.frag.
constexpr const char* kLightingTail2 = R"GLSL(
    vec3 color = ambient + Lo + emissive;
    color = color / (color + vec3(1.0));

    outColor = vec4(color, alpha);
}
)GLSL";

std::string glslVarName(const std::string& id)
{
    std::string out = "v_";
    out.reserve(out.size() + id.size());
    for (char c : id)
        out += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
    return out;
}

const char* typeKeyword(ValueType t)
{
    switch (t) {
    case ValueType::Float: return "float";
    case ValueType::Vec2:  return "vec2";
    case ValueType::Vec3:  return "vec3";
    case ValueType::Vec4:  return "vec4";
    }
    return "float";
}

std::string formatFloat(float v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    std::string s(buf);
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find("inf") == std::string::npos && s.find("nan") == std::string::npos) {
        s += ".0";
    }
    return s;
}

std::string constantLiteral(ValueType t, const glm::vec4& v)
{
    switch (t) {
    case ValueType::Float: return formatFloat(v.x);
    case ValueType::Vec2:  return "vec2(" + formatFloat(v.x) + ", " + formatFloat(v.y) + ")";
    case ValueType::Vec3:  return "vec3(" + formatFloat(v.x) + ", " + formatFloat(v.y) + ", " + formatFloat(v.z) + ")";
    case ValueType::Vec4:  return "vec4(" + formatFloat(v.x) + ", " + formatFloat(v.y) + ", " + formatFloat(v.z) + ", " + formatFloat(v.w) + ")";
    }
    return "0.0";
}

const char* samplerNameFor(TextureSlot s)
{
    switch (s) {
    case TextureSlot::BaseColor:         return "baseColorTex";
    case TextureSlot::MetallicRoughness: return "metallicRoughnessTex";
    case TextureSlot::Normal:            return "normalTex";
    case TextureSlot::Occlusion:         return "occlusionTex";
    case TextureSlot::Emissive:          return "emissiveTex";
    }
    return "baseColorTex";
}

std::string mathExpr(MathOp op, const std::string& a, const std::string& b)
{
    switch (op) {
    case MathOp::Add:      return a + " + " + b;
    case MathOp::Subtract: return a + " - " + b;
    case MathOp::Multiply: return a + " * " + b;
    case MathOp::Divide:   return a + " / " + b;
    case MathOp::Min:      return "min(" + a + ", " + b + ")";
    case MathOp::Max:      return "max(" + a + ", " + b + ")";
    }
    return a;
}

// Mirrors PhmatGraph.cpp's file-local parseValueTypeName() -- duplicated rather than shared
// through a header since it is a trivial string<->enum mapping local to each parser (PhmatGraph.cpp
// for ".phmat" node fields, this file for ".phshader" fields).
bool parseValueTypeName(const std::string& s, ValueType& out)
{
    if (s == "float") { out = ValueType::Float; return true; }
    if (s == "vec2")  { out = ValueType::Vec2;  return true; }
    if (s == "vec3")  { out = ValueType::Vec3;  return true; }
    if (s == "vec4")  { out = ValueType::Vec4;  return true; }
    return false;
}

uint64_t fnv1a64(const std::string& s)
{
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

} // namespace

namespace {

// Returns the .phshader source registered for a Custom node's id, and whether its own declared
// signature actually matches what the node itself declares (PhmatGraph.h's comment on why Custom
// nodes redundantly declare customInputTypes/customOutputType explains why this cross-check has
// to happen here, at compile time, rather than in PhmatGraph.cpp's filesystem-free validate step).
bool findMatchingPhshader(const PhmatNode& n, const std::unordered_map<std::string, PhshaderSource>& phshaders,
                           const PhshaderSource** outSrc)
{
    *outSrc = nullptr;
    auto it = phshaders.find(n.id);
    if (it == phshaders.end()) return false;
    const PhshaderSource& src = it->second;
    if (src.outputType != n.customOutputType) return false;
    if (src.inputs.size() != n.customInputTypes.size()) return false;
    for (size_t k = 0; k < src.inputs.size(); ++k) {
        if (src.inputs[k].type != n.customInputTypes[k]) return false;
    }
    *outSrc = &src;
    return true;
}

} // namespace

bool compileGraphToGlsl(const PhmatGraph& graph, const std::vector<std::string>& topoOrder,
                         const std::unordered_map<std::string, PhshaderSource>& phshaders,
                         std::string& outGlsl, std::vector<PhshaderSplice>& outSplices,
                         std::vector<PhmatDiagnostic>& outDiagnostics)
{
    outGlsl.clear();
    outSplices.clear();
    bool ok = true;

    std::unordered_map<std::string, const PhmatNode*> byId;
    for (const auto& n : graph.nodes) byId[n.id] = &n;

    if (!byId.count(graph.outputNode)) {
        outDiagnostics.push_back({PhmatDiagnostic::Severity::Error, graph.outputNode,
            "internal error: output node not found (call validateAndSort() first)"});
        return false;
    }

    // --- Pass 1: emit Custom-node function declarations, "before main()" ----------------------
    // A shared .phshader (same functionName referenced by more than one Custom node) is only
    // emitted once; every referencing node still gets its own outSplices entry, all pointing at
    // that one shared line range.
    std::string functionsText;
    {
        const std::string headerText(kGltfPbrHeader);
        int line = 2 + static_cast<int>(std::count(headerText.begin(), headerText.end(), '\n'));
        auto append = [&](const std::string& text) {
            functionsText += text;
            line += static_cast<int>(std::count(text.begin(), text.end(), '\n'));
        };

        struct FunctionLineRange { std::string phshaderPath; int firstBodyLine; int lastBodyLine; };
        std::unordered_map<std::string, FunctionLineRange> functionLineRange; // functionName -> range
        for (const auto& id : topoOrder) {
            auto it = byId.find(id);
            if (it == byId.end()) continue; // reported as a diagnostic in pass 2 below
            const PhmatNode& n = *it->second;
            if (n.type != NodeType::Custom) continue;

            const PhshaderSource* src = nullptr;
            if (!findMatchingPhshader(n, phshaders, &src)) continue; // reported as a diagnostic in pass 2 below
            if (functionLineRange.count(src->functionName)) continue; // shared .phshader, function already emitted

            if (!src->helpers.empty()) {
                append(src->helpers);
                if (functionsText.back() != '\n') append("\n");
            }

            std::string sig = std::string(typeKeyword(src->outputType)) + " " + src->functionName + "(";
            for (size_t k = 0; k < src->inputs.size(); ++k) {
                if (k) sig += ", ";
                sig += std::string(typeKeyword(src->inputs[k].type)) + " " + src->inputs[k].name;
            }
            sig += ") {\n";
            append(sig);

            const int firstBodyLine = line;
            const int lastBodyLine = line + static_cast<int>(std::count(src->body.begin(), src->body.end(), '\n'));
            functionLineRange[src->functionName] = { n.customPhshaderPath, firstBodyLine, lastBodyLine };

            append(src->body);
            if (src->body.empty() || src->body.back() != '\n') append("\n");
            append("}\n\n");
        }

        for (const auto& id : topoOrder) {
            auto it = byId.find(id);
            if (it == byId.end()) continue;
            const PhmatNode& n = *it->second;
            if (n.type != NodeType::Custom) continue;
            const PhshaderSource* src = nullptr;
            if (!findMatchingPhshader(n, phshaders, &src)) continue;
            auto rangeIt = functionLineRange.find(src->functionName);
            if (rangeIt == functionLineRange.end()) continue;
            outSplices.push_back(PhshaderSplice{ id, rangeIt->second.phshaderPath,
                                                  rangeIt->second.firstBodyLine, rangeIt->second.lastBodyLine });
        }
    }

    // --- Pass 2: main() body, in topological order ---------------------------------------------
    // Recomputes each node's output type -- mirrors PhmatGraph.cpp's validateAndSort(), which this
    // function assumes already ran successfully (so a type mismatch here is not expected other
    // than the Custom-node/.phshader signature cross-check below, which validateAndSort() cannot
    // perform itself since it never touches the filesystem).
    std::unordered_map<std::string, ValueType> types;
    auto typeOf = [&](const std::string& id) { return types.at(id); };

    std::string body;
    for (const auto& id : topoOrder) {
        auto it = byId.find(id);
        if (it == byId.end()) {
            outDiagnostics.push_back({PhmatDiagnostic::Severity::Error, id,
                "internal error: topo order references an unknown node id"});
            return false;
        }
        const PhmatNode& n = *it->second;
        const std::string var = glslVarName(id);

        switch (n.type) {
        case NodeType::Constant:
            types[id] = n.constantType;
            body += "    " + std::string(typeKeyword(n.constantType)) + " " + var + " = " +
                    constantLiteral(n.constantType, n.constantValue) + ";\n";
            break;
        case NodeType::TextureSlot:
            types[id] = ValueType::Vec4;
            body += "    vec4 " + var + " = texture(" + samplerNameFor(n.textureSlot) + ", fragTexCoord);\n";
            break;
        case NodeType::Uv:
            types[id] = ValueType::Vec2;
            body += "    vec2 " + var + " = fragTexCoord;\n";
            break;
        case NodeType::NormalMap:
            types[id] = ValueType::Vec3;
            body += "    vec3 " + var + ";\n";
            body += "    {\n";
            body += "        vec3 tn = texture(normalTex, fragTexCoord).xyz * 2.0 - 1.0;\n";
            body += "        tn.xy *= " + glslVarName(n.normalMapScale) + ";\n";
            body += "        mat3 TBN = mat3(normalize(fragTangent), normalize(fragBitangent), normalize(fragNormal));\n";
            body += "        " + var + " = normalize(TBN * tn);\n";
            body += "    }\n";
            break;
        case NodeType::Mix:
            types[id] = typeOf(n.mixA);
            body += "    " + std::string(typeKeyword(types[id])) + " " + var + " = mix(" +
                    glslVarName(n.mixA) + ", " + glslVarName(n.mixB) + ", " + glslVarName(n.mixFactor) + ");\n";
            break;
        case NodeType::Math:
            types[id] = typeOf(n.mathA);
            body += "    " + std::string(typeKeyword(types[id])) + " " + var + " = " +
                    mathExpr(n.mathOp, glslVarName(n.mathA), glslVarName(n.mathB)) + ";\n";
            break;
        case NodeType::Custom: {
            types[id] = n.customOutputType;
            const PhshaderSource* src = nullptr;
            if (!findMatchingPhshader(n, phshaders, &src)) {
                outDiagnostics.push_back({PhmatDiagnostic::Severity::Error, id,
                    "\"" + n.customPhshaderPath + "\" is missing, or does not declare the inputs/output this node expects"});
                ok = false;
                body += "    " + std::string(typeKeyword(n.customOutputType)) + " " + var + " = " +
                        typeKeyword(n.customOutputType) + "(0.0);\n";
                break;
            }
            std::string call = src->functionName + "(";
            for (size_t k = 0; k < n.customInputs.size(); ++k) {
                if (k) call += ", ";
                call += glslVarName(n.customInputs[k]);
            }
            call += ")";
            body += "    " + std::string(typeKeyword(n.customOutputType)) + " " + var + " = " + call + ";\n";
            break;
        }
        case NodeType::PbrOutput:
            types[id] = ValueType::Vec4;
            break; // terminal -- consumed below, not declared as its own local
        }
    }

    if (!ok) return false;

    const PhmatNode& outNode = *byId.at(graph.outputNode);
    const std::string baseColorExpr = glslVarName(outNode.pbrBaseColor);
    const std::string metallicExpr  = glslVarName(outNode.pbrMetallic);
    const std::string roughnessExpr = glslVarName(outNode.pbrRoughness);
    const std::string normalExpr    = outNode.pbrNormal.empty()    ? std::string("normalize(fragNormal)") : glslVarName(outNode.pbrNormal);
    const std::string occlusionExpr = outNode.pbrOcclusion.empty() ? std::string("1.0")                   : glslVarName(outNode.pbrOcclusion);
    const std::string emissiveExpr  = outNode.pbrEmissive.empty()  ? std::string("vec3(0.0)")              : glslVarName(outNode.pbrEmissive);
    const std::string alphaExpr     = outNode.pbrAlpha.empty()     ? std::string("baseColor.a")            : glslVarName(outNode.pbrAlpha);

    outGlsl = std::string(kGltfPbrHeader) + "\n" + functionsText + "void main() {\n" + body +
        "\n    vec4 baseColor = " + baseColorExpr + ";\n" +
        "    float alpha = " + alphaExpr + ";\n" +
        "    if (alpha < 0.01) discard;\n\n" +
        "    float metallic  = clamp(" + metallicExpr + ", 0.0, 1.0);\n" +
        "    float roughness = clamp(" + roughnessExpr + ", 0.04, 1.0);\n\n" +
        "    vec3 N = " + normalExpr + ";\n" +
        std::string(kLightingTail1) +
        "    float occlusion = " + occlusionExpr + ";\n" +
        "    ambient *= occlusion;\n" +
        "\n    vec3 emissive = " + emissiveExpr + ";\n" +
        std::string(kLightingTail2);

    return true;
}

bool parsePhshader(const std::string& jsonText, PhshaderSource& out, std::string& outError)
{
    out = PhshaderSource{};

    Json root = Json::parse(jsonText, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded() || !root.is_object()) {
        outError = "malformed JSON document";
        return false;
    }

    out.version = root.value("version", 0);
    if (out.version != 1) {
        outError = "unsupported .phshader version (only version 1 is recognized)";
        return false;
    }

    out.functionName = root.value("functionName", "");
    if (out.functionName.empty() ||
        std::isdigit(static_cast<unsigned char>(out.functionName.front()))) {
        outError = "\"functionName\" must be a non-empty GLSL identifier not starting with a digit";
        return false;
    }
    for (char c : out.functionName) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
            outError = "\"functionName\" must only contain letters, digits, and underscores";
            return false;
        }
    }

    if (!root.contains("output") || !root["output"].is_string() ||
        !parseValueTypeName(root.value("output", ""), out.outputType)) {
        outError = "missing/invalid \"output\" type (float/vec2/vec3/vec4)";
        return false;
    }

    if (root.contains("inputs")) {
        if (!root["inputs"].is_array()) {
            outError = "\"inputs\" must be an array";
            return false;
        }
        for (const Json& j : root["inputs"]) {
            if (!j.is_object() || !j.contains("name") || !j["name"].is_string() ||
                !j.contains("type") || !j["type"].is_string()) {
                outError = "each \"inputs\" entry needs a string \"name\" and \"type\"";
                return false;
            }
            PhshaderInput in;
            in.name = j.value("name", "");
            if (in.name.empty() ||
                !(std::isalpha(static_cast<unsigned char>(in.name.front())) || in.name.front() == '_')) {
                outError = "each \"inputs\" entry's \"name\" must be a valid GLSL identifier";
                return false;
            }
            if (!parseValueTypeName(j.value("type", ""), in.type)) {
                outError = "each \"inputs\" entry's \"type\" must be float/vec2/vec3/vec4";
                return false;
            }
            out.inputs.push_back(std::move(in));
        }
    }

    out.helpers = root.value("helpers", "");
    out.body = root.value("body", "");
    if (out.body.empty()) {
        outError = "missing/empty \"body\"";
        return false;
    }

    return true;
}

bool loadPhshaderFile(const std::string& phshaderPath, PhshaderSource& out, std::string& outError)
{
    std::ifstream in(phshaderPath, std::ios::binary);
    if (!in) {
        outError = "cannot open .phshader file: " + phshaderPath;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parsePhshader(ss.str(), out, outError);
}

bool findGlslcPath(std::string& outPath)
{
    const char* sdk = std::getenv("VULKAN_SDK");
    if (!sdk || !*sdk) return false;

    namespace fs = std::filesystem;
#ifdef _WIN32
    fs::path p = fs::path(sdk) / "Bin" / "glslc.exe";
#else
    fs::path p = fs::path(sdk) / "bin" / "glslc";
#endif
    std::error_code ec;
    if (!fs::exists(p, ec)) return false;
    outPath = p.string();
    return true;
}

bool compileGlslToSpirv(const std::string& glslSource, const std::string& cacheDir,
                         std::vector<uint32_t>& outSpirv, std::string& outErrorLog)
{
    outSpirv.clear();
    outErrorLog.clear();

    std::string glslc;
    if (!findGlslcPath(glslc)) {
        outErrorLog = "glslc not found (VULKAN_SDK not set, or glslc missing under it)";
        return false;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    char hashHex[17];
    std::snprintf(hashHex, sizeof(hashHex), "%016llx",
                  static_cast<unsigned long long>(fnv1a64(glslSource)));

    const fs::path fragPath = fs::path(cacheDir) / (std::string("phmat_") + hashHex + ".frag");
    const fs::path spvPath  = fs::path(cacheDir) / (std::string("phmat_") + hashHex + ".spv");
    const fs::path logPath  = fs::path(cacheDir) / (std::string("phmat_") + hashHex + ".log");

    if (fs::exists(spvPath, ec)) {
        std::vector<uint32_t> cached = Phantom::VKG::loadSPV(spvPath.string());
        if (!cached.empty()) {
            outSpirv = std::move(cached);
            return true; // cache hit -- glslc not invoked
        }
        // an empty/corrupt cached file is treated as a miss and regenerated below
    }

    {
        std::ofstream f(fragPath, std::ios::binary | std::ios::trunc);
        if (!f) {
            outErrorLog = "failed to write GLSL cache file: " + fragPath.string();
            return false;
        }
        f << glslSource;
    }

    const std::string cmd = "\"" + glslc + "\" -fshader-stage=frag \"" + fragPath.string() +
                             "\" -o \"" + spvPath.string() + "\" > \"" + logPath.string() + "\" 2>&1";
#ifdef _WIN32
    // cmd.exe's /c argument has a well-known quirk: when the command string starts with a
    // quote character (as ours does -- glslc's own path is quoted), cmd looks for the LAST
    // quote in the whole line and strips both, which here spans across our separately-quoted
    // frag/spv/log path arguments and mangles the command. Wrapping the entire string in one
    // more outer pair of quotes gives cmd.exe exactly that extra pair to strip, leaving our
    // real quoting intact underneath -- the standard workaround for system()/cmd.exe (POSIX
    // system()'s /bin/sh has no such quirk, so this is Windows-only).
    const std::string wrappedCmd = "\"" + cmd + "\"";
    const int rc = std::system(wrappedCmd.c_str());
#else
    const int rc = std::system(cmd.c_str());
#endif

    {
        std::ifstream logIn(logPath, std::ios::binary);
        if (logIn) {
            std::ostringstream ss;
            ss << logIn.rdbuf();
            outErrorLog = ss.str();
        }
    }

    if (rc != 0) return false;

    outSpirv = Phantom::VKG::loadSPV(spvPath.string());
    return !outSpirv.empty();
}

namespace {

// Rewrites glslc's error log (plain text, one diagnostic per line shaped like
// "<path>:<line>: error: <message>" -- see compileGlslToSpirv()'s fragPath) into PhmatDiagnostics.
// A line whose number falls inside a PhshaderSplice's body range is attributed to that node's own
// .phshader file, with the line number rewritten to be relative to that file's own "body" field
// (plan item 4 "error位置を表示する") instead of the generated shader's line number, which a
// .phmat/.phshader author never sees. Anything glslc emits that isn't itself a "<path>:<line>:
// (error|warning):" line (e.g. a trailing "N error(s) generated." summary) is dropped -- it adds
// no location information beyond what the per-diagnostic lines already carried.
std::vector<PhmatDiagnostic> rewriteCompileErrorLog(const std::string& errorLog, const std::vector<PhshaderSplice>& splices)
{
    std::vector<PhmatDiagnostic> diags;
    std::istringstream lines(errorLog);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;

        PhmatDiagnostic::Severity sev = PhmatDiagnostic::Severity::Error;
        size_t markerPos = line.find(": error:");
        if (markerPos == std::string::npos) {
            markerPos = line.find(": warning:");
            sev = PhmatDiagnostic::Severity::Warning;
        }
        if (markerPos == std::string::npos) continue; // not a per-diagnostic line (e.g. a summary count)

        size_t numStart = markerPos;
        while (numStart > 0 && std::isdigit(static_cast<unsigned char>(line[numStart - 1]))) --numStart;
        if (numStart == markerPos || numStart == 0 || line[numStart - 1] != ':') continue; // unexpected shape -- skip rather than misreport

        const int generatedLine = std::atoi(line.substr(numStart, markerPos - numStart).c_str());
        const std::string message = line.substr(markerPos + 2); // skip the leading ": "

        const PhshaderSplice* hit = nullptr;
        for (const auto& sp : splices) {
            if (generatedLine >= sp.firstBodyLineInGenerated && generatedLine <= sp.lastBodyLineInGenerated) {
                hit = &sp;
                break;
            }
        }
        if (hit) {
            const int localLine = generatedLine - hit->firstBodyLineInGenerated + 1;
            diags.push_back({sev, hit->nodeId, hit->phshaderPath + ":" + std::to_string(localLine) + ": " + message});
        } else {
            diags.push_back({sev, "", "generated shader:" + std::to_string(generatedLine) + ": " + message});
        }
    }
    if (diags.empty()) diags.push_back({PhmatDiagnostic::Severity::Error, "", "glslc compile failed: " + errorLog});
    return diags;
}

} // namespace

PhmatLoadResult loadPhmatMaterial(const std::string& phmatPath, const std::string& cacheDir)
{
    PhmatLoadResult result;

    std::ifstream in(phmatPath, std::ios::binary);
    if (!in) {
        result.diagnostics.push_back({PhmatDiagnostic::Severity::Error, "", "cannot open .phmat file: " + phmatPath});
        return result;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();

    PhmatGraph graph;
    if (!parsePhmatGraph(text, graph, result.diagnostics)) return result;

    std::vector<std::string> topoOrder;
    if (!validateAndSort(graph, topoOrder, result.diagnostics)) return result;

    // Load every Custom node's ".phshader" file, resolved relative to phmatPath's own directory
    // (mirrors how the rest of this codebase resolves asset-relative paths against the referencing
    // file/project rather than embedding absolute paths).
    namespace fs = std::filesystem;
    const fs::path baseDir = fs::path(phmatPath).parent_path();
    std::unordered_map<std::string, PhshaderSource> phshaders;
    bool phshadersOk = true;
    for (const auto& n : graph.nodes) {
        if (n.type != NodeType::Custom) continue;
        PhshaderSource src;
        std::string err;
        if (!loadPhshaderFile((baseDir / n.customPhshaderPath).string(), src, err)) {
            result.diagnostics.push_back({PhmatDiagnostic::Severity::Error, n.id, "\"" + n.customPhshaderPath + "\": " + err});
            phshadersOk = false;
            continue;
        }
        phshaders.emplace(n.id, std::move(src));
    }
    if (!phshadersOk) return result;

    std::string glsl;
    std::vector<PhshaderSplice> splices;
    if (!compileGraphToGlsl(graph, topoOrder, phshaders, glsl, splices, result.diagnostics)) return result;

    std::string errorLog;
    if (!compileGlslToSpirv(glsl, cacheDir, result.fragSpirv, errorLog)) {
        auto rewritten = rewriteCompileErrorLog(errorLog, splices);
        result.diagnostics.insert(result.diagnostics.end(), rewritten.begin(), rewritten.end());
        result.fragSpirv.clear();
        return result;
    }

    // Reflect the actually-compiled SPIR-V (plan item 4 "descriptor/push constantをreflectionで
    // 検証する") -- a Custom node's .phshader body could otherwise declare a resource binding or
    // push constant GltfSceneRenderer's fixed material pipeline layout does not provide, which
    // would only surface much later as a Vulkan validation error (or worse) at pipeline-creation
    // time. reflectSpirv() returning false here would mean glslc produced something that does not
    // even start with the SPIR-V magic number -- a glslc/loadSPV bug, not a shader authoring
    // mistake -- so that case is deliberately not treated as a load failure.
    PhmatReflectionResult reflection;
    if (reflectSpirv(result.fragSpirv, reflection)) {
        std::vector<std::string> violations;
        if (!validateReflection(reflection, violations)) {
            for (const auto& v : violations) result.diagnostics.push_back({PhmatDiagnostic::Severity::Error, "", v});
            result.fragSpirv.clear();
            return result;
        }
    }

    result.success = true;
    return result;
}

}
