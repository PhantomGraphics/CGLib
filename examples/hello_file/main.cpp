// Minimal CGLib::File consumer -- parses a tiny in-memory OBJ and checks the
// vertex count, exercising a target that pulls in Math + Graphics transitively.
#include "CGLib/File/File/OBJFileReader.h"

#include <sstream>
#include <cstdio>

int main()
{
    std::istringstream obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");

    Phantom::File::OBJFileReader reader;
    if (!reader.read(obj)) {
        std::puts("OBJ parse failed");
        return 1;
    }
    const auto positions = reader.getOBJ().positions;
    std::printf("vertices = %zu\n", positions.size());
    return positions.size() == 3 ? 0 : 1;
}
