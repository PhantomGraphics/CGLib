#version 450
layout(std140,binding=1) uniform Uniforms {
    mat4 vp; mat4 inverseVP; mat4 reflectionVP;
    vec4 eye; vec4 domain; vec4 color; vec4 absorption; vec4 flags; uvec4 grid;
} u;
layout(binding=2) uniform sampler2D reflectionImage;
layout(binding=3) uniform sampler2D sceneImage;
layout(binding=4) uniform sampler2D sceneDepth;
layout(location=0) in vec3 world;
layout(location=1) in vec3 normal;
layout(location=0) out vec4 outColor;
void main() {
    vec3 n=normalize(normal), v=normalize(u.eye.xyz-world);
    vec4 clip=u.vp*vec4(world,1), reflected=u.reflectionVP*vec4(world,1);
    vec2 uv=clip.xy/clip.w*0.5+0.5;
    vec2 distorted=clamp(uv+n.xz*u.absorption.w,vec2(0.001),vec2(0.999));
    float depth=texture(sceneDepth,distorted).r;
    vec4 bottom=u.inverseVP*vec4(distorted*2-1,depth,1);
    bottom/=bottom.w;
    float thickness=max(world.y-bottom.y,0);
    vec3 transmission=exp(-u.absorption.rgb*thickness);
    vec3 refracted=mix(u.color.rgb,texture(sceneImage,distorted).rgb,transmission);
    vec2 ruv=clamp(reflected.xy/reflected.w*0.5+0.5+n.xz*u.absorption.w,vec2(0.001),vec2(0.999));
    vec3 reflection=texture(reflectionImage,ruv).rgb;
    float fresnel=0.02+0.98*pow(1-max(dot(n,v),0),5);
    vec3 base=u.flags.y>0.5?refracted:u.color.rgb;
    if(u.flags.x>0.5) base=mix(base,reflection,fresnel);
    vec3 l=normalize(vec3(-0.4,1,0.3));
    float spec=pow(max(dot(n,normalize(l+v)),0),96);
    outColor=vec4(base*(0.8+0.2*max(dot(n,l),0))+vec3(spec*0.6),1);
}
