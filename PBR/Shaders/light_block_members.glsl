    vec4 ambient;         // rgb = colour * intensity
    vec4 lightInfo;       // x = light count
    vec4 lightVec[4];     // xyz = direction to the light (w=0) or position (w=1)
    vec4 lightColor[4];   // rgb = colour * intensity; w = cos(inner cone) for spot lights
    vec4 lightDir[4];     // spot: xyz = direction the light travels, w = cos(outer cone); w <= -1.5 = not a spot
    mat4 shadowVP;        // world -> clip space of the shadow-casting light
    vec4 shadowInfo;      // x = slot of the light that casts (-1 none), y + w * distance = texel size in world units, z = depth bias
    vec4 areaU[4];        // area lights: xyz = half-width axis in world space (w = 1); w = 0 for every other light
    vec4 areaV[4];        // area lights: xyz = half-height axis
