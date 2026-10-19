#version 450

// Ray-casts the sphere (ellipsoid after the model's scale) of one point and writes its depth, so points intersect meshes and
// each other like real geometry. The ray is solved in the point's local space: with B = inverse(mat3(mv)) the view-space
// ray p = s*d meets |B (p - centre)| = r where  a s^2 - 2 b s + c = 0,  q = B d,  w = B centre.
layout(location = 0) in vec3 fragColor;
layout(location = 1) flat in vec3 viewCenter;
layout(location = 2) flat in float localRadius;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PC {
    mat4 mv;
    vec4 tint;
    vec4 proj;
    vec4 viewport;
    vec4 params;
} pc;

void main() {
    vec2 ndc = (gl_FragCoord.xy - pc.viewport.xy) / pc.viewport.zw * 2.0 - 1.0;
    vec3 d = vec3(ndc.x / pc.proj.x, ndc.y / pc.proj.y, -1.0);
    mat3 B = inverse(mat3(pc.mv));
    vec3 q = B * d;
    vec3 w = B * viewCenter;
    float a = dot(q, q);
    float b = dot(q, w);
    float c = dot(w, w) - localRadius * localRadius;
    float disc = b * b - a * c;
    if (disc < 0.0) discard;
    float s = (b - sqrt(disc)) / a;           // the surface nearer to the eye
    if (!(s > 0.0)) discard;                  // behind the eye (the eye is inside the sphere)
    vec3 p = s * d;
    gl_FragDepth = (pc.proj.z * p.z + pc.proj.w) / (-p.z);

    // Surface normal of the ellipsoid in view space: B^T (B (p - centre)).
    vec3 n = normalize(transpose(B) * (B * (p - viewCenter)));
    float facing = clamp(dot(n, -normalize(p)), 0.0, 1.0);
    float shade = mix(0.55, 1.0, facing);     // headlight, never darker than 55 %
    vec3 rgb = fragColor * shade;
    if (pc.params.w > 0.5) rgb = pow(max(rgb, vec3(0.0)), vec3(1.0 / 2.2));
    outColor = vec4(rgb, 1.0);
}
