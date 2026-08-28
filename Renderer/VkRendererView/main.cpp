#include "VkRendererApp.h"

int main(int argc, char* argv[]) {
    VKRenderer::VkRendererApp app(1280, 720, "VkRendererView");
    app.run(argc, argv);
    return 0;
}
