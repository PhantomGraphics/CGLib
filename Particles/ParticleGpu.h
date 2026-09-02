#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>

namespace Phantom::Particles {

// GPU-side particle layout (std430, 64 bytes). Must match the `Particle` struct in
// shaders/particle_update.comp and shaders/particle_billboard.vert exactly.
struct ParticleGpu {
    glm::vec4 position; // xyz = world position, w = life (1 = just spawned, 0 = dead)
    glm::vec4 velocity; // xyz = velocity, w = billboard size
    glm::vec4 color;    // rgba at spawn time
    glm::vec4 colorEnd; // rgba at death (lerped against color by life in the vertex shader)
};

static_assert(sizeof(ParticleGpu) == 64, "ParticleGpu layout changed -- update the GLSL Particle struct accordingly");

enum class ParticleEmitterShape : uint32_t { Point = 0, Sphere = 1, Box = 2 };

// CPU-side emitter configuration. Mirrors internal design notes Phase B.
struct EmitterParams {
    glm::vec3 origin        = { 0.f, 0.f, 0.f };
    float     emitRate      = 50.f;  // particles/sec
    float     lifeTime      = 2.0f;  // seconds
    float     speed         = 1.0f;
    float     speedVariance = 0.2f;
    float     size          = 0.05f;
    glm::vec3 initialColor  = { 1.f, 0.7f, 0.3f };
    glm::vec3 endColor      = { 0.1f, 0.1f, 0.1f };
    glm::vec3 gravity       = { 0.f, -9.8f, 0.f };
    float     drag          = 0.1f;

    ParticleEmitterShape shape       = ParticleEmitterShape::Point;
    float                shapeRadius = 0.1f;
};

} // namespace Phantom::Particles
