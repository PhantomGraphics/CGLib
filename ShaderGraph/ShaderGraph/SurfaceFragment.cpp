#include "RuntimeCompiler.h"

namespace Phantom::ShaderGraph {

namespace {

// Everything before the generated graph code. Must stay in sync with
// CGApp/PhantomStudio/shaders/cgstudio.vert (outputs) and PhantomStudioRenderer.h
// (StudioPushConstants).
const char* kPrefix =
    "#version 450\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    mat4 model;\n"
    "    vec4 baseColor;\n"
    "    vec4 flags;\n"
    "} pc;\n"
    "layout(location = 0) in vec3 fragNormal;\n"
    "layout(location = 1) in vec3 fragPos;\n"
    "layout(location = 2) in vec2 fragUV;\n"
    "layout(location = 3) in vec2 fragNormalUV;\n"
    "layout(location = 4) in vec4 fragTangent;\n"
    "layout(location = 0) out vec4 outColor;\n";

// Tangent frame (same derivation as cgstudio.frag), then the surface evaluation and a
// minimal lit output. The metallic-roughness lighting function replaces the
// Lambert term here in Phase 3.
const char* kSuffix =
    "void main() {\n"
    "    vec3 N = normalize(fragNormal);\n"
    "    vec3 dpdx = dFdx(fragPos), dpdy = dFdy(fragPos);\n"
    "    vec2 duvdx = dFdx(fragNormalUV), duvdy = dFdy(fragNormalUV);\n"
    "    float det = duvdx.x * duvdy.y - duvdx.y * duvdy.x;\n"
    "    vec3 T = fragTangent.xyz - N * dot(N, fragTangent.xyz);\n"
    "    float handedness = fragTangent.w;\n"
    "    if (dot(T, T) < 1e-12 && abs(det) > 1e-10) {\n"
    "        T = (dpdx * duvdy.y - dpdy * duvdx.y) / det;\n"
    "        vec3 B = (dpdy * duvdx.x - dpdx * duvdy.x) / det;\n"
    "        T -= N * dot(N, T);\n"
    "        handedness = dot(cross(N, T), B) < 0.0 ? -1.0 : 1.0;\n"
    "    }\n"
    "    vec3 B2 = dot(T, T) > 1e-12 ? cross(N, normalize(T)) * handedness : cross(N, abs(N.x) < 0.9 ? vec3(1,0,0) : vec3(0,1,0));\n"
    "    vec3 T2 = dot(T, T) > 1e-12 ? normalize(T) : normalize(cross(B2, N));\n"
    "    SGSurface s;\n"
    "    sg_evaluate(fragUV, mat3(T2, normalize(B2), N), s);\n"
    "    vec3 L = normalize(vec3(1.0, 2.0, 1.0));\n"
    "    float ndl = max(dot(s.normal, L), 0.0);\n"
    "    vec3 color = 0.15 * s.baseColor + ndl * s.baseColor;\n"
    "    outColor = vec4(pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);\n"
    "}\n";

int countLines(const char* s) {
    int n = 0;
    for (; *s; ++s) n += (*s == '\n');
    return n;
}

}  // namespace

SurfaceFragment buildSurfaceFragment(const CompileResult& graph) {
    SurfaceFragment f;
    f.graphLineOffset = countLines(kPrefix);
    f.source = std::string(kPrefix) + graph.glsl + kSuffix;
    return f;
}

NodeId nodeForFragmentLine(const SurfaceFragment& f, const CompileResult& graph, int line) {
    return nodeForGlslLine(graph, line - f.graphLineOffset);
}

}  // namespace Phantom::ShaderGraph
