#include "VulkanImGuiApp.h"
#include <vk_utils.h>
#include <vector>
#include <set>
#include <iostream>

void VulkanImGuiApp::pickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    auto deviceScore = [&](VkPhysicalDevice d) -> int {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(d, &props);
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
        if (!isDeviceSuitable(d, surface_)) return -1; // discard if not suitable
        return score;
    };

    int bestScore = -1;
    VkPhysicalDevice best = VK_NULL_HANDLE;
    for (auto d : devices) {
        int s = deviceScore(d);
        if (s > bestScore) { bestScore = s; best = d; }
    }
    if (!best) throw std::runtime_error("Failed to find suitable GPU");
    physicalDevice_ = best;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    std::cout << "[Vulkan] Using GPU: " << props.deviceName << std::endl;
}

void VulkanImGuiApp::createLogicalDevice()
{
    auto indices = findQueueFamilies(physicalDevice_, surface_);
    std::set<uint32_t> uniqueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (uint32_t fam : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceFeatures features{};

    const std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos = qcis.data();
    dci.pEnabledFeatures = &features;
    dci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    dci.ppEnabledExtensionNames = extensions.data();

    vkutils::checkVk(vkCreateDevice(physicalDevice_, &dci, nullptr, &device_), "vkCreateDevice failed");

    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

VulkanImGuiApp::QueueFamilyIndices VulkanImGuiApp::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
            indices.presentFamily = i;
        if (indices.isComplete()) break;
    }
    return indices;
}

bool VulkanImGuiApp::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    auto indices = findQueueFamilies(device, surface);

    // Check required device extensions (swapchain)
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, available.data());

    const std::vector<const char*> required = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    for (auto* req : required) {
        bool found = false;
        for (auto& e : available) if (strcmp(e.extensionName, req) == 0) { found = true; break; }
        if (!found) return false;
    }

    // Basic swapchain capabilities check
    uint32_t fmtCount = 0, pmCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmtCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &pmCount, nullptr);

    return indices.isComplete() && fmtCount > 0 && pmCount > 0;
}

