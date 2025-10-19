#include "VulkanImGuiApp.h"
#include <GLFW/glfw3.h>
#include <vk_utils.h>
#include <vector>
#include <cstring>
#include <iostream>

namespace {
static VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    (void)messageType; (void)pUserData;
    const char* sev =
        (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" :
        (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN" :
        (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO" : "VERBOSE";
    std::cerr << "[Vulkan-" << sev << "] " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}
}

void VulkanImGuiApp::createInstance()
{
    std::vector<const char*> layers;
    const bool enableValidation = wantValidationLayers();
#ifndef NDEBUG
    if (enableValidation) {
        layers = { "VK_LAYER_KHRONOS_validation" };
        if (!checkValidationLayerSupport(layers)) layers.clear();
    }
#endif

    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "RogueLikeGame";
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName = "NoEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    auto exts = getRequiredExtensions(!layers.empty());

    VkInstanceCreateInfo ci{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();
    ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    vkutils::checkVk(vkCreateInstance(&ci, nullptr, &instance_), "vkCreateInstance failed");
}

void VulkanImGuiApp::setupDebugMessenger()
{
#ifndef NDEBUG
    auto vkCreateDebugUtilsMessengerEXT_ptr = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (!vkCreateDebugUtilsMessengerEXT_ptr) return;

    VkDebugUtilsMessengerCreateInfoEXT ci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = vkDebugCallback;
    ci.pUserData = nullptr;

    VkResult res = vkCreateDebugUtilsMessengerEXT_ptr(instance_, &ci, nullptr, &debugMessenger_);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan debug messenger, VkResult=" << res << std::endl;
    }
#endif
}

void VulkanImGuiApp::destroyDebugMessenger()
{
#ifndef NDEBUG
    if (!debugMessenger_) return;
    auto vkDestroyDebugUtilsMessengerEXT_ptr = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (vkDestroyDebugUtilsMessengerEXT_ptr)
        vkDestroyDebugUtilsMessengerEXT_ptr(instance_, debugMessenger_, nullptr);
    debugMessenger_ = VK_NULL_HANDLE;
#endif
}

bool VulkanImGuiApp::wantValidationLayers()
{
#ifndef NDEBUG
    return true;
#else
    return false;
#endif
}

std::vector<const char*> VulkanImGuiApp::getRequiredExtensions(bool enableValidation)
{
    uint32_t count = 0;
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> exts(glfwExt, glfwExt + count);
    if (enableValidation) {
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return exts;
}

bool VulkanImGuiApp::checkValidationLayerSupport(const std::vector<const char*>& layers)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());
    for (auto* name : layers) {
        bool found = false;
        for (auto& p : available) {
            if (strcmp(p.layerName, name) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}
