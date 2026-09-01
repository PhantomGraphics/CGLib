#version 450

// Must match Phantom::Particles::ParticleGpu (ParticleGpu.h) exactly.
struct Particle {
    vec4 position;
    vec4 velocity;
    vec4 color;
    vec4 colorEnd;
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

layout(std140, binding = 1) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec4 camRight; // xyz, world space
    vec4 camUp;    // xyz, world space
} cam;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outLocalUV;

const vec2 kCorners[6] = vec2[6](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
    vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5)
);

void main()
{
    uint particleIndex = uint(gl_VertexIndex) / 6u;
    uint corner         = uint(gl_VertexIndex) % 6u;

    Particle p = particles[particleIndex];
    float life = p.position.w;
    vec2  localUV = kCorners[corner];

    vec3 worldPos = p.position.xyz
        + cam.camRight.xyz * localUV.x * p.velocity.w
        + cam.camUp.xyz    * localUV.y * p.velocity.w;

    gl_Position = cam.proj * cam.view * vec4(worldPos, 1.0);

    outColor   = (life > 0.0) ? mix(p.colorEnd, p.color, clamp(life, 0.0, 1.0)) : vec4(0.0);
    outLocalUV = localUV + 0.5; // 0..1
}
