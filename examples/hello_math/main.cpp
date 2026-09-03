// Minimal CGLib::Math consumer -- verifies the public include convention
// (`#include "CGLib/..."`) and the namespaced link target resolve.
#include "CGLib/Math/Vector3d.h"

#include <cstdio>

int main()
{
    const Phantom::Math::Vector3df v(3.0f, 4.0f, 0.0f);
    const float len = Phantom::Math::getLength(v);
    std::printf("|v| = %.1f\n", len);
    return (len > 4.9f && len < 5.1f) ? 0 : 1;
}
