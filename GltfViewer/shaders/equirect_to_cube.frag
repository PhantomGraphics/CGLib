#version 450

// Projects a cube face's local direction onto an equirectangular (lat-long) 2D panorama
// texture -- the standard direction -> UV formula, assuming row 0 of the source image is its
// zenith (see Phantom::Graphics::HDRImageFileReader::read()'s comment for why the loader does
// not vertically flip .hdr files). Consumed by GltfIBLPrecomputer::computeEnvironmentCube()
// via the same generic cube-face render pass irradiance.frag/prefilter.frag use, just with a
// sampler2D binding instead of samplerCube.

layout(location = 0) in vec3 inLocalPos;

layout(set = 0, binding = 0) uniform sampler2D equirectMap;

layout(location = 0) out vec4 outColor;

const vec2 kInvAtan = vec2(0.1591549, 0.3183099); // 1/(2*PI), 1/PI

vec2 directionToEquirectUV(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= kInvAtan;
    uv.x += 0.5;
    uv.y = 0.5 - uv.y; // v.y = +1 (straight up) -> uv.y = 0 (top row = zenith)
    return uv;
}

void main() {
    vec3 dir = normalize(inLocalPos);
    outColor = vec4(texture(equirectMap, directionToEquirectUV(dir)).rgb, 1.0);
}
