#version 450

// Simplified luma-edge-detection FXAA (loosely based on the public-domain "FXAA 3.11
// console quality" writeups). Computes luma inline from the LDR input rather than reading
// it from a precomputed alpha channel, trading a small amount of quality for not requiring
// upstream passes to pack luma into alpha.

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
    vec2  texelSize;
    float contrastThreshold;
    float relativeThreshold;
    float subpixelBlend;
} ubo;

layout(binding = 1) uniform sampler2D uInput;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main()
{
    vec3 colorCenter = texture(uInput, inUV).rgb;

    float lumaN  = luma(textureOffset(uInput, inUV, ivec2( 0,-1)).rgb);
    float lumaS  = luma(textureOffset(uInput, inUV, ivec2( 0, 1)).rgb);
    float lumaE  = luma(textureOffset(uInput, inUV, ivec2( 1, 0)).rgb);
    float lumaW  = luma(textureOffset(uInput, inUV, ivec2(-1, 0)).rgb);
    float lumaM  = luma(colorCenter);

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    float range   = lumaMax - lumaMin;

    if (range < max(ubo.contrastThreshold, lumaMax * ubo.relativeThreshold)) {
        outColor = vec4(colorCenter, 1.0);
        return;
    }

    float lumaNW = luma(textureOffset(uInput, inUV, ivec2(-1,-1)).rgb);
    float lumaNE = luma(textureOffset(uInput, inUV, ivec2( 1,-1)).rgb);
    float lumaSW = luma(textureOffset(uInput, inUV, ivec2(-1, 1)).rgb);
    float lumaSE = luma(textureOffset(uInput, inUV, ivec2( 1, 1)).rgb);

    // Edge direction from the Sobel-like gradient of the 3x3 neighborhood.
    float edgeHoriz = abs(lumaNW + lumaNE - 2.0 * lumaN)
                    + 2.0 * abs(lumaW + lumaE - 2.0 * lumaM)
                    + abs(lumaSW + lumaSE - 2.0 * lumaS);
    float edgeVert  = abs(lumaNW + lumaSW - 2.0 * lumaW)
                    + 2.0 * abs(lumaN + lumaS - 2.0 * lumaM)
                    + abs(lumaNE + lumaSE - 2.0 * lumaE);
    bool isHorizontal = edgeHoriz >= edgeVert;

    // Blend towards the direction (of the two perpendicular neighbors) with higher contrast.
    float luma1 = isHorizontal ? lumaN : lumaW;
    float luma2 = isHorizontal ? lumaS : lumaE;
    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);
    float stepUV = isHorizontal ? ubo.texelSize.y : ubo.texelSize.x;
    bool towards2 = gradient2 >= gradient1;

    vec2 blendUV = inUV;
    if (isHorizontal) blendUV.y += towards2 ? stepUV : -stepUV;
    else              blendUV.x += towards2 ? stepUV : -stepUV;

    vec3 blended = texture(uInput, blendUV).rgb;
    float blendAmount = ubo.subpixelBlend * clamp(range / max(lumaMax, 1e-4), 0.0, 1.0);

    outColor = vec4(mix(colorCenter, blended, blendAmount), 1.0);
}
