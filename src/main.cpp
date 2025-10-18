#include "app/VulkanImGuiApp.h"
#include <string>

int main(int argc, char** argv) {
    VulkanImGuiApp app;
    if (argc > 1 && argv[1] && std::string(argv[1]) == "--smoke") {
        return app.runSmokeTest();
    }
    return app.run();
}
