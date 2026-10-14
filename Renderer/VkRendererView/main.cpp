#include "RendererApp.h"

int main(int argc, char* argv[]) {
    Phantom::VKRenderer::RendererApp app(1280, 720, "VkRendererView");
    app.run(argc, argv);
    return 0;
}
