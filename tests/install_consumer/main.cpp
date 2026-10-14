// Uses only the installed package: headers via "CGLib/...", libraries via CGLib::<Component>.
#include "CGLib/Math/Ellipse3d.h"
#include "CGLib/Math/SafeNormalize.h"
#include "CGLib/Math/Statistics.h"
#include "CGLib/Math/Vector3d.h"
#include "CGLib/Numerics/Numerics/SVD3d.h"
#include "CGLib/Space/Space/BVH.h"
#include "CGLib/Volume/Volume/Volume.h"
#include "CGLib/File/File/OBJFileReader.h"
#include "CGLib/Animation/Animation/Animator.h"
#include "CGLib/GeometryNode/GeometryNode/GeometryOps.h"
#include "CGLib/SceneRuntime/SceneRuntime/SceneGraph.h"

#include <cstdio>
#include <sstream>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
        if (!ok) ++failures;
    };

    const Phantom::Math::Vector3df v(3.0f, 4.0f, 0.0f);
    check(Phantom::Math::getLength(v) > 4.9f, "Math");

    // Math contracts: try* APIs fail without touching `out`, and succeed on valid input.
    Phantom::Math::Vector3df normal(9.0f, 9.0f, 9.0f);
    const Phantom::Math::Ellipse3df flat({0, 0, 0}, {2, 0, 0}, {0, 3, 0});
    const Phantom::Math::Ellipse3df degenerate({0, 0, 0}, {1, 0, 0}, {2, 0, 0});
    check(!degenerate.tryGetNormal(normal) && normal.x == 9.0f, "Math tryGetNormal failure keeps out");
    check(flat.tryGetNormal(normal) && normal.z > 0.999f, "Math tryGetNormal");
    float mean = 0.0f;
    check(!Phantom::Math::Statistics<float>().tryGetAverage(mean), "Math Statistics empty fails");
    check(Phantom::Math::Statistics<float>({1.0f, 3.0f}).tryGetAverage(mean) && mean == 2.0f, "Math Statistics average");

    Phantom::Numerics::SVD3d svd;
    Phantom::Math::Matrix3dd identity(1.0);
    check(svd.calculate(identity).isOk, "Numerics (Eigen bundled)");

    std::istringstream obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Phantom::File::OBJFileReader reader;
    check(reader.read(obj) && reader.getOBJ().positions.size() == 3, "File");

    Phantom::GeometryNode::Mesh terrain;
    check(Phantom::GeometryNode::makeTerrain(Phantom::GeometryNode::TerrainParams{}, Phantom::GeometryNode::Limits{}, terrain) == Phantom::GeometryNode::OpStatus::Ok, "GeometryNode terrain");

    Phantom::SceneRuntime::SceneGraph graph;
    check(graph.size() == 0, "SceneRuntime (nlohmann bundled)");

    return failures == 0 ? 0 : 1;
}
