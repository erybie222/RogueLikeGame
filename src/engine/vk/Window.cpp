#include "engine/gfx/VulkanImGuiApp.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>


void VulkanImGuiApp::initWindow()
{
    if (!glfwInit())
        throw std::runtime_error("GLFW init failed");

    // (opcjonalnie, ale polecam mieć)
    glfwSetErrorCallback([](int err, const char* desc)
                         { std::cerr << "GLFW Error " << err << ": " << desc << std::endl; });

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create a borderless window sized and positioned to cover the primary monitor
    // (windowed fullscreen / borderless window). This allows alt-tabbing and using
    // other monitors without the exclusive-fullscreen behavior.
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor)
        throw std::runtime_error("No primary monitor");

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode)
        throw std::runtime_error("No video mode");

    // Create a windowed (not exclusive fullscreen) window at the monitor resolution
    window_ = glfwCreateWindow(mode->width, mode->height, "RogueLikeGame", nullptr, nullptr);
    if (!window_)
        throw std::runtime_error("Failed to create window");

    // Position the window at the top-left of the primary monitor so it covers it
    int monX = 0, monY = 0;
    glfwGetMonitorPos(monitor, &monX, &monY);
    glfwSetWindowPos(window_, monX, monY);

    glfwSetWindowUserPointer(window_, this);
}
