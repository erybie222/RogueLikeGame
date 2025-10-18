#include "VulkanImGuiApp.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stdexcept>
#include <cstdio>

void VulkanImGuiApp::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(window_, true);

    auto indices = findQueueFamilies(physicalDevice_, surface_);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_1;
    init_info.Instance = instance_;
    init_info.PhysicalDevice = physicalDevice_;
    init_info.Device = device_;
    init_info.QueueFamily = indices.graphicsFamily.value();
    init_info.Queue = graphicsQueue_;
    init_info.DescriptorPool = imguiDescriptorPool_;
    init_info.RenderPass = renderPass_;
    init_info.MinImageCount = static_cast<uint32_t>(swapchainImages_.size());
    init_info.ImageCount = static_cast<uint32_t>(swapchainImages_.size());
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = [](VkResult err) {
        if (err != VK_SUCCESS) fprintf(stderr, "ImGui Vulkan error: %d\n", err);
    };

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("ImGui_ImplVulkan_Init failed");
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        throw std::runtime_error("ImGui_ImplVulkan_CreateFontsTexture failed");
    }
}

void VulkanImGuiApp::reinitImGuiRenderer()
{
    // Shutdown only Vulkan backend objects
    ImGui_ImplVulkan_Shutdown();

    auto indices = findQueueFamilies(physicalDevice_, surface_);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_1;
    init_info.Instance = instance_;
    init_info.PhysicalDevice = physicalDevice_;
    init_info.Device = device_;
    init_info.QueueFamily = indices.graphicsFamily.value();
    init_info.Queue = graphicsQueue_;
    init_info.DescriptorPool = imguiDescriptorPool_;
    init_info.RenderPass = renderPass_;
    init_info.MinImageCount = static_cast<uint32_t>(swapchainImages_.size());
    init_info.ImageCount = static_cast<uint32_t>(swapchainImages_.size());
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = [](VkResult err) {
        if (err != VK_SUCCESS) fprintf(stderr, "ImGui Vulkan error: %d\n", err);
    };

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("ImGui_ImplVulkan_Init (reinit) failed");
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        throw std::runtime_error("ImGui_ImplVulkan_CreateFontsTexture (reinit) failed");
    }
}
