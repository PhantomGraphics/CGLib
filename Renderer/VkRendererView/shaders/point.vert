#version 450

// --- 頂点入力 (binding 0, 1, 2 は別バッファ) ---
layout(location = 0) in vec3  inPos;
layout(location = 1) in vec4  inColor;
layout(location = 2) in float inSize;

// --- UBO ---
layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
} ubo;

// --- フラグメントシェーダへ ---
layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position  = ubo.mvp * vec4(inPos, 1.0);
    gl_PointSize = inSize;
    fragColor    = inColor;
}
