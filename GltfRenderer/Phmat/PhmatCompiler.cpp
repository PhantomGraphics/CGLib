#include "PhmatCompiler.h"

#include "../../../CGLib/VulkanGraphics/VulkanSPVLoader.h"

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

bool compileGraphToGlsl(const PhmatGraph& graph, const std::vector<std::string>& topoOrder,
                         std::string& outGlsl, std::vector<PhmatDiagnostic>& outDiagnostics)
{
    outGlsl.clear();

    std::unordered_map<std::string, const PhmatNode*> byId;
    for (const auto& n : graph.nodes) byId[n.id] = &n;

    if (!byId.count(graph.outputNode)) {
        outDiagnostics.push_back({PhmatDiagnostic::Severity::Error, graph.outputNode,
            "internal error: output node not found (call validateAndSort() first)"});
        return false;
    }

    // Recomputes each node's output type in topological order -- mirrors PhmatGraph.cpp's
    // validateAndSort(), which this function assumes already ran successfully (so no type
    // mismatch is expected to actually occur here).
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
        case NodeType::PbrOutput:
            types[id] = ValueType::Vec4;
            break; // terminal -- consumed below, not declared as its own local
        }
    }

    const PhmatNode& outNode = *byId.at(graph.outputNode);
    const std::string baseColorExpr = glslVarName(outNode.pbrBaseColor);
    const std::string metallicExpr  = glslVarName(outNode.pbrMetallic);
    const std::string roughnessExpr = glslVarName(outNode.pbrRoughness);
    const std::string normalExpr    = outNode.pbrNormal.empty()    ? std::string("normalize(fragNormal)") : glslVarName(outNode.pbrNormal);
    const std::string occlusionExpr = outNode.pbrOcclusion.empty() ? std::string("1.0")                   : glslVarName(outNode.pbrOcclusion);
    const std::string emissiveExpr  = outNode.pbrEmissive.empty()  ? std::string("vec3(0.0)")              : glslVarName(outNode.pbrEmissive);
    const std::string alphaExpr     = outNode.pbrAlpha.empty()     ? std::string("baseColor.a")            : glslVarName(outNode.pbrAlpha);

    outGlsl = std::string(kGltfPbrHeader) + "\nvoid main() {\n" + body +
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

    std::string glsl;
    if (!compileGraphToGlsl(graph, topoOrder, glsl, result.diagnostics)) return result;

    std::string errorLog;
    if (!compileGlslToSpirv(glsl, cacheDir, result.fragSpirv, errorLog)) {
        result.diagnostics.push_back({PhmatDiagnostic::Severity::Error, "", "glslc compile failed: " + errorLog});
        result.fragSpirv.clear();
        return result;
    }

    result.success = true;
    return result;
}

}
