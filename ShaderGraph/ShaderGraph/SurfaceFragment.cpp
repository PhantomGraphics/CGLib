#include "RuntimeCompiler.h"

namespace Phantom::ShaderGraph {

namespace {

// Everything before the generated graph code. Must stay in sync with
// CGApp/PhantomStudio/shaders/cgstudio.vert (outputs), PhantomStudioRenderer.h
// (StudioCameraUBO, StudioPushConstants).
const char* kPrefix =
    "#version 450\n"
    "layout(set = 0, binding = 0) uniform CameraUBO {\n"
    "    mat4 view;\n"
    "    mat4 proj;\n"
    "    vec3 camPos;\n"
    "    float _pad;\n"
    "    vec4 ambient;\n"        // rgb = colour * intensity
    "    vec4 lightInfo;\n"      // x = light count
    "    vec4 lightVec[4];\n"    // xyz = direction to the light (w = 0) or position (w = 1)
    "    vec4 lightColor[4];\n"  // rgb = colour * intensity; w = cos(inner cone) for spot lights
    "    vec4 lightDir[4];\n"    // spot: xyz = direction the light travels, w = cos(outer cone); w <= -1.5 = not a spot
    "    mat4 shadowVP;\n"       // world -> clip space of the shadow-casting light
    "    vec4 shadowInfo;\n"     // x = slot of the light that casts (-1 none), y + w * distance = texel size, z = depth bias
    "    vec4 areaU[4];\n"      // area lights: xyz = half-width axis (w = 1); w = 0 for every other light
    "    vec4 areaV[4];\n"      // area lights: xyz = half-height axis
    "} cam;\n"
    "layout(set = 0, binding = 1) uniform sampler2D shadowMap;\n"
    "float shadowVisibility(vec3 pos, vec3 n, int slot) {\n"  // 3x3 PCF, normal-offset bias (same as cgstudio.frag)
    "    if (cam.shadowInfo.x < -0.5 || int(cam.shadowInfo.x + 0.5) != slot) return 1.0;\n"
    "    float dist = cam.lightVec[slot].w > 0.5 ? distance(pos, cam.lightVec[slot].xyz) : 0.0;\n"
    "    float texel = cam.shadowInfo.y + cam.shadowInfo.w * dist;\n"
    "    vec4 p = cam.shadowVP * vec4(pos + n * texel * 1.5, 1.0);\n"
    "    vec3 c = p.xyz / p.w;\n"
    "    vec2 uv = c.xy * 0.5 + 0.5;\n"
    "    if (c.z >= 1.0 || c.z < 0.0 || uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return 1.0;\n"
    "    float lit = 0.0;\n"
    "    for (int y = -1; y <= 1; ++y)\n"
    "        for (int x = -1; x <= 1; ++x)\n"
    "            lit += (c.z - cam.shadowInfo.z <= texture(shadowMap, uv + vec2(x, y) / 2048.0).r) ? 1.0 : 0.0;\n"
    "    return lit / 9.0;\n"
    "}\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    mat4 model;\n"
    "    vec4 baseColor;\n"  // unused by graphs
    "    vec4 flags;\n"      // w = 1 when the object is selected
    "} pc;\n"
    "layout(location = 0) in vec3 fragNormal;\n"
    "layout(location = 1) in vec3 fragPos;\n"
    "layout(location = 2) in vec2 fragUV;\n"
    "layout(location = 3) in vec2 fragNormalUV;\n"
    "layout(location = 4) in vec4 fragTangent;\n"
    "layout(location = 0) out vec4 outColor;\n";

// Tangent frame (same derivation as cgstudio.frag), the surface evaluation, then one directional
// light + ambient through a metallic-roughness GGX model (Schlick Fresnel, Smith-Schlick
// geometry). Light radiance is pi so that a rough dielectric matches the Lambert look of
// cgstudio.frag; the output encode is the same pow(1/2.2).
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
    "    vec3 base = mix(s.baseColor, vec3(1.0, 0.85, 0.1), 0.4 * pc.flags.w);\n"
    "    float metallic = s.metallic;\n"
    "    float rough = max(s.roughness, 0.04);\n"
    "    vec3 n = normalize(s.normal);\n"
    "    vec3 V = normalize(cam.camPos - fragPos);\n"
    "    vec3 F0 = mix(vec3(0.04), base, metallic);\n"
    "    float a = rough * rough, a2 = a * a;\n"
    "    float k = (rough + 1.0) * (rough + 1.0) / 8.0;\n"
    "    float ndv = max(dot(n, V), 1e-4);\n"
    "    vec3 lit = vec3(0.0);\n"
    "    int nLights = int(cam.lightInfo.x + 0.5);\n"
    "    for (int i = 0; i < 4; ++i) {\n"
    "        if (i >= nLights) break;\n"
    "        vec3 L = cam.lightVec[i].xyz;\n"
    "        vec3 radiance = cam.lightColor[i].rgb;\n"
    "        if (cam.lightVec[i].w > 0.5) {\n"  // point light: inverse-square falloff
    "            vec3 d = L - fragPos;\n"
    "            float dist2 = max(dot(d, d), 1e-4);\n"
    "            L = d * inversesqrt(dist2);\n"
    "            radiance /= dist2;\n"
    "            if (cam.lightDir[i].w > -1.5) {\n"  // spot cone
    "                float cosA = dot(normalize(cam.lightDir[i].xyz), -L);\n"
    "                radiance *= smoothstep(cam.lightDir[i].w, max(cam.lightColor[i].w, cam.lightDir[i].w + 1e-4), cosA);\n"
    "            }\n"
    "        }\n"
    "        if (cam.areaU[i].w > 0.5) {\n"
    "            vec3 U = cam.areaU[i].xyz, Vv = cam.areaV[i].xyz;\n"
    "            vec3 travel = normalize(cross(Vv, U));\n"
    "            float avg = 0.0;\n"
    "            for (int iy = 0; iy < 4; ++iy)\n"
    "                for (int ix = 0; ix < 4; ++ix) {\n"
    "                    vec2 sp = (vec2(ix, iy) + 0.5) / 4.0 * 2.0 - 1.0;\n"
    "                    vec3 d = cam.lightVec[i].xyz + U * sp.x + Vv * sp.y - fragPos;\n"
    "                    float dist2 = max(dot(d, d), 1e-4);\n"
    "                    avg += max(dot(travel, -d * inversesqrt(dist2)), 0.0) / dist2;\n"
    "                }\n"
    "            L = cam.lightVec[i].xyz - fragPos;\n"
    "            radiance = cam.lightColor[i].rgb * (avg / 16.0);\n"
    "        }\n"
    "        L = normalize(L);\n"
    "        vec3 H = normalize(V + L);\n"
    "        float ndl = max(dot(n, L), 0.0), ndh = max(dot(n, H), 0.0), vdh = max(dot(V, H), 0.0);\n"
    "        vec3 F = F0 + (1.0 - F0) * pow(1.0 - vdh, 5.0);\n"
    "        float dd = ndh * ndh * (a2 - 1.0) + 1.0;\n"
    "        float D = a2 / (3.14159265 * dd * dd);\n"
    "        float G = (ndl / (ndl * (1.0 - k) + k)) * (ndv / (ndv * (1.0 - k) + k));\n"
    "        vec3 spec = D * G * F / max(4.0 * ndl * ndv, 1e-4);\n"
    "        vec3 diffuse = (1.0 - F) * (1.0 - metallic) * base / 3.14159265;\n"
    "        lit += (diffuse + spec) * 3.14159265 * ndl * radiance * shadowVisibility(fragPos, normalize(fragNormal), i);\n"
    "    }\n"
    "    float sky = n.y * 0.5 + 0.5;\n"
    "    vec3 envSpec = F0 * mix(0.25, 0.9, sky) * (1.0 - 0.6 * rough);\n"  // cheap hemispherical environment
    "    vec3 ambient = cam.ambient.rgb * (1.0 - metallic) * base + (cam.ambient.rgb / 0.15) * 0.5 * envSpec;\n"
    "    vec3 color = ambient + lit;\n"
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
