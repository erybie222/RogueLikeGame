#include "engine/gfx/VulkanImGuiApp.h"

#include <string>

int main(int argc, char** argv)
{
    VulkanImGuiApp app;

    for (int i = 1; i < argc; ++i)
    {
        if (!argv[i])
            continue;

        const std::string arg = argv[i];

        if (arg == "--smoke")
        {
            
            return app.runSmokeTest();
        }

        if (arg == "--ai-mode")
        {
            app.enableAiMode();
        }
    }
    return app.run();
}
