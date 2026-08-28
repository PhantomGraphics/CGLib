#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform PC {
    mat4  lightVP;
    float layerFar; // upper bound of this layer's light-space NDC depth, in [0,1]
} pc;

layout(location = 0) out float outDensity;

void main() {
    // alpha=0 marks GPU-generated padding slots (see pbvr_render.vert); cull them the same way.
    if (inColor.a == 0.0) {
        gl_Position  = vec4(0.0, 0.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        outDensity   = 0.0;
        return;
    }

    vec4 clip = pc.lightVP * vec4(inPos, 1.0);
    float ndcZ = clip.z / clip.w;

    // Particles beyond this layer's far cutoff are excluded from this layer's accumulation
    // (each layer accumulates only what lies between the light and its own depth cutoff).
    if (ndcZ > pc.layerFar) {
        gl_Position  = vec4(0.0, 0.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        outDensity   = 0.0;
        return;
    }

    gl_Position  = clip;
    gl_PointSize = 3.0;
    outDensity   = inColor.a;
}
