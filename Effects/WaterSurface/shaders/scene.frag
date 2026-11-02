#version 450
layout(push_constant) uniform Camera { mat4 inverseVP; mat4 vp; } camera;
layout(location=0) out vec4 color;
layout(location=0) in vec2 uv;
#ifndef CLIP_MODE
#define CLIP_MODE 0
#endif
// Camera-space rays make this scene usable with the mirrored camera too.
void main() {
    vec4 a=camera.inverseVP*vec4(uv*2-1,0,1), b=camera.inverseVP*vec4(uv*2-1,1,1);
    vec3 origin=a.xyz/a.w, direction=normalize(b.xyz/b.w-origin);
    float nearest=1e6;
    vec3 tint=vec3(0.35,0.55,0.75);
    if(CLIP_MODE!=1 && abs(direction.y)>1e-6) {
        float t=(-1.5-origin.y)/direction.y;
        vec3 hit=origin+direction*t;
        if(t>0 && abs(hit.x-5)<9 && abs(hit.z-5)<9) {
            nearest=t;
            tint=mix(vec3(0.55,0.48,0.3),vec3(0.8,0.75,0.55),mod(floor(hit.x)+floor(hit.z),2));
        }
    }
    for(int i=0;i<3;++i) {
        vec3 lo=vec3(2+float(i)*2,CLIP_MODE==1?0.001:-1.5,3+float(i%2)*3);
        vec3 hi=lo+vec3(0.7,3.5,0.7);
        if(CLIP_MODE==2) hi.y=-0.001;
        else hi.y=2;
        vec3 t0=(lo-origin)/direction,t1=(hi-origin)/direction;
        vec3 low=min(t0,t1), high=max(t0,t1);
        float enter=max(max(low.x,low.y),low.z), leave=min(min(high.x,high.y),high.z);
        if(enter>0 && leave>=enter && enter<nearest) {
            nearest=enter; tint=vec3(0.8,0.2+float(i)*0.2,0.15);
        }
    }
    if(nearest<1e6) {
        vec4 clip=camera.vp*vec4(origin+direction*nearest,1);
        gl_FragDepth=clip.z/clip.w;
    } else gl_FragDepth=1;
    color=vec4(tint,1);
}
