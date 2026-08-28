#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;

// Layout must match Crystal::Volume::PBVRPipeline::UBO (PBVRPipeline.h).
layout(binding = 0) uniform UBO {
    mat4 mvp;
    float particleSize;
    float _pad0;
    float _pad1;
    float _pad2;
    mat4 lightVP;
    float sigma;
    float layerCount;
    float shadowEnabled;
} ubo;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 lightClipPos;

void main() {
    // alpha=0 marks GPU-generated padding slots; cull them beyond the far plane.
    if (inColor.a == 0.0) {
        gl_Position  = vec4(0.0, 0.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        fragColor    = vec4(0.0);
        lightClipPos = vec4(0.0);
        return;
    }
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
    gl_PointSize = ubo.particleSize;
    fragColor = inColor;
    lightClipPos = ubo.lightVP * vec4(inPos, 1.0);
}
