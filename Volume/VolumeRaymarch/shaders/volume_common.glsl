// Shared by volume_raymarch.frag and volume_sun_transmittance.comp.
// Mirrors CGLib/Volume/Volume/VolumeScattering.cpp (the CPU reference).

bool intersectBox(vec3 o, vec3 d, vec3 bmin, vec3 bmax, out float tNear, out float tFar)
{
    float t0 = 0.0;
    float t1 = 1.0e30;
    for (int a = 0; a < 3; ++a) {
        if (abs(d[a]) < 1.0e-12) {
            if (o[a] < bmin[a] || o[a] > bmax[a]) return false;
            continue;
        }
        float ta = (bmin[a] - o[a]) / d[a];
        float tb = (bmax[a] - o[a]) / d[a];
        if (ta > tb) { float t = ta; ta = tb; tb = t; }
        t0 = max(t0, ta);
        t1 = min(t1, tb);
        if (t0 > t1) return false;
    }
    tNear = t0;
    tFar = t1;
    return true;
}

float henyeyGreenstein(float cosTheta, float g)
{
    float denom = 1.0 + g * g - 2.0 * g * cosTheta;
    return (1.0 - g * g) / (4.0 * 3.14159265358979 * pow(max(denom, 1.0e-6), 1.5));
}
