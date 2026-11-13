#include "Texture.h"
#include <iostream>
int main(int argc, char** argv) {
    using namespace Phantom::Texture;
    std::string diagnostic; Recipe recipe; Image image;
    if (argc==3 && std::string(argv[1])=="--example") {
        if (save(std::filesystem::u8path(argv[2]),example(),diagnostic)) return 0;
    } else if (argc==3) {
        if (load(std::filesystem::u8path(argv[1]),recipe,diagnostic) && evaluate(recipe,image,diagnostic) &&
            writePng(std::filesystem::u8path(argv[2]),image,diagnostic)) return 0;
    } else { std::cerr<<"Usage: TextureCLI --example recipe.texture.json\n       TextureCLI recipe.texture.json output.png\n"; return 2; }
    std::cerr<<diagnostic<<'\n'; return 1;
}
