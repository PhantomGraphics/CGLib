// Uses only the installed package: headers via "CGLib/...", libraries via CGLib::<Component>.
#include "CGLib/Math/Vector3d.h"
#include "CGLib/Numerics/Numerics/SVD3d.h"
#include "CGLib/Space/Space/BVH.h"
#include "CGLib/Volume/Volume/Volume.h"
#include "CGLib/File/File/OBJFileReader.h"
#include "CGLib/Animation/Animation/Animator.h"
#include "CGLib/Terrain/Terrain/TerrainGenerator.h"
#include "CGLib/Scene/Scene/SceneBase.h"
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

    Phantom::Numerics::SVD3d svd;
    Phantom::Math::Matrix3dd identity(1.0);
    check(svd.calculate(identity).isOk, "Numerics (Eigen bundled)");

    std::istringstream obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Phantom::File::OBJFileReader reader;
    check(reader.read(obj) && reader.getOBJ().positions.size() == 3, "File");

    Phantom::Terrain::TerrainSettings terrain;
    check(Phantom::Terrain::validate(terrain) == Phantom::Terrain::TerrainError::None, "Terrain");

    Phantom::SceneRuntime::SceneGraph graph;
    check(graph.size() == 0, "SceneRuntime (nlohmann bundled)");

    return failures == 0 ? 0 : 1;
}
