// Viewport light evaluation shared by cgstudio.frag and the shader-graph surface fragment (CGLib PBR).
// Needs, declared before this text: a uniform block instance `cam` whose members are light_block_members.glsl (the C++ side is
// Phantom::PBR::LightBlock), and `uniform sampler2D shadowMap`.

// Fraction of the light reaching `pos` (1 = lit): 3x3 percentage-closer filtering with a normal-offset bias.
float shadowVisibility(vec3 pos, vec3 n, int slot) {
    if (cam.shadowInfo.x < -0.5 || int(cam.shadowInfo.x + 0.5) != slot) return 1.0;
    float dist = cam.lightVec[slot].w > 0.5 ? distance(pos, cam.lightVec[slot].xyz) : 0.0;
    float texel = cam.shadowInfo.y + cam.shadowInfo.w * dist;
    vec4 p = cam.shadowVP * vec4(pos + n * texel * 1.5, 1.0);
    vec3 c = p.xyz / p.w;
    vec2 uv = c.xy * 0.5 + 0.5;
    if (c.z >= 1.0 || c.z < 0.0 || uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return 1.0;
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            lit += (c.z - cam.shadowInfo.z <= texture(shadowMap, uv + vec2(x, y) / 2048.0).r) ? 1.0 : 0.0;
    return lit / 9.0;
}

// Direction to light `i` (unit; toward its centre for a point / spot / area light) and its radiance at `pos`: colour * intensity,
// attenuated by inverse-square distance, the spot cone, and for an area light the rectangle's averaged one-sided falloff.
void evalLight(int i, vec3 pos, out vec3 L, out vec3 radiance) {
    L = cam.lightVec[i].xyz;
    radiance = cam.lightColor[i].rgb;
    if (cam.lightVec[i].w > 0.5) {                // point light: inverse-square falloff
        vec3 d = L - pos;
        float dist2 = max(dot(d, d), 1e-4);
        L = d * inversesqrt(dist2);
        radiance /= dist2;
        if (cam.lightDir[i].w > -1.5) {           // spot cone: smooth fade between the inner and outer angle
            float cosA = dot(normalize(cam.lightDir[i].xyz), -L);
            radiance *= smoothstep(cam.lightDir[i].w, max(cam.lightColor[i].w, cam.lightDir[i].w + 1e-4), cosA);
        }
    }
    if (cam.areaU[i].w > 0.5) {                   // area light: a 4x4 grid of point samples over the rectangle, one-sided
        vec3 U = cam.areaU[i].xyz, V = cam.areaV[i].xyz;
        vec3 travel = normalize(cross(V, U));
        float avg = 0.0;
        for (int iy = 0; iy < 4; ++iy)
            for (int ix = 0; ix < 4; ++ix) {
                vec2 s = (vec2(ix, iy) + 0.5) / 4.0 * 2.0 - 1.0;
                vec3 d = cam.lightVec[i].xyz + U * s.x + V * s.y - pos;
                float dist2 = max(dot(d, d), 1e-4);
                avg += max(dot(travel, -d * inversesqrt(dist2)), 0.0) / dist2;
            }
        L = cam.lightVec[i].xyz - pos;
        radiance = cam.lightColor[i].rgb * (avg / 16.0);
    }
    L = normalize(L);
}

// radiance * max(dot(N, L), 0) for a Lambert surface. An area light integrates N.L over the rectangle's samples, which keeps the
// light on surfaces that face the rectangle's edge (a single centre direction would cut it off).
vec3 lightIrradiance(int i, vec3 pos, vec3 N) {
    if (cam.areaU[i].w > 0.5) {
        vec3 U = cam.areaU[i].xyz, V = cam.areaV[i].xyz;
        vec3 travel = normalize(cross(V, U));
        float total = 0.0;
        for (int iy = 0; iy < 4; ++iy)
            for (int ix = 0; ix < 4; ++ix) {
                vec2 s = (vec2(ix, iy) + 0.5) / 4.0 * 2.0 - 1.0;
                vec3 d = cam.lightVec[i].xyz + U * s.x + V * s.y - pos;
                float dist2 = max(dot(d, d), 1e-4);
                vec3 Ls = d * inversesqrt(dist2);
                total += max(dot(N, Ls), 0.0) * max(dot(travel, -Ls), 0.0) / dist2;
            }
        return cam.lightColor[i].rgb * (total / 16.0);
    }
    vec3 L, radiance;
    evalLight(i, pos, L, radiance);
    return radiance * max(dot(N, L), 0.0);
}
