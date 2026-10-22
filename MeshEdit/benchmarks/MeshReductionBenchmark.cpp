// Explicit Release-only benchmark. Never registered with CTest or GoogleTest.
#include "../Core/MeshReduction.h"
#include "../Core/EditableMeshConvert.h"
#include "../../Graphics/Core/PrimitiveMesh.h"
#include <chrono>
#include <cstdio>

int main() {
#ifndef NDEBUG
    std::fprintf(stderr, "Build this benchmark in Release.\n");
    return 1;
#else
    using namespace Phantom::MeshEdit;
    EditableMesh input, output;
    std::string error;
    if (!makeEditableFromMeshData(Phantom::Graphics::buildSphere(226, 224, 1), input, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str()); return 1;
    }
    MeshReductionReport report;
    const auto begin = std::chrono::steady_clock::now();
    if (!reduceMesh(input, {}, output, report, &error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::printf("triangles=%zu -> %zu targetReached=%d work=%zu seconds=%.3f\n",
        report.inputTriangles, report.outputTriangles, report.targetReached ? 1 : 0, report.work, seconds);
    return report.targetReached ? 0 : 1;
#endif
}
