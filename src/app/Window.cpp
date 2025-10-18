#include "VulkanImGuiApp.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "[GLFW] Error " << error << ": " << description << std::endl;
}

void VulkanImGuiApp::initWindow()
{
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");
    if (!glfwVulkanSupported()) throw std::runtime_error("GLFW reports Vulkan not supported");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(1280, 720, "RogueLikeGame", nullptr, nullptr);
    if (!window_) throw std::runtime_error("Failed to create GLFW window");
}
