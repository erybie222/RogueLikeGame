#include "VulkanImGuiApp.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace {
    void checkVk(VkResult res, const char* msg) {
        if (res != VK_SUCCESS) {
            std::cerr << msg << " VkResult=" << res << std::endl;
            throw std::runtime_error(msg);
        }
    }

    void glfwErrorCallback(int error, const char* description) {
        std::cerr << "[GLFW] Error " << error << ": " << description << std::endl;
    }

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

int VulkanImGuiApp::run()
{
    try {
        initWindow();
        initVulkan();
        initImGui();
        mainLoop();
        vkDeviceWaitIdle(device_);
        cleanup();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        cleanup();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int VulkanImGuiApp::runSmokeTest()
{
    try {
        std::cout << "[Smoke] Init window..." << std::endl;
        initWindow();
        std::cout << "[Smoke] Init Vulkan..." << std::endl;
        initVulkan();
        std::cout << "[Smoke] Init ImGui..." << std::endl;
        initImGui();
        vkDeviceWaitIdle(device_);
        std::cout << "[Smoke] OK" << std::endl;
        cleanup();
    } catch (const std::exception& e) {
        std::cerr << "[Smoke] Failed: " << e.what() << std::endl;
        cleanup();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void VulkanImGuiApp::initWindow()
{
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");
    if (!glfwVulkanSupported()) throw std::runtime_error("GLFW reports Vulkan not supported");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(1280, 720, "RogueLikeGame + ImGui + Vulkan", nullptr, nullptr);
    if (!window_) throw std::runtime_error("Failed to create GLFW window");
}

void VulkanImGuiApp::initVulkan()
{
    createInstance(/*enableValidation*/ true);
    setupDebugMessenger();
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createRenderPass();
    createFramebuffers();
    createCommandPoolAndBuffers();
    createSyncObjects();
    createDescriptorPoolForImGui();
}

void VulkanImGuiApp::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

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
        if (err != VK_SUCCESS) std::cerr << "ImGui Vulkan error: " << err << std::endl;
    };

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("ImGui_ImplVulkan_Init failed");
    }

    // Upload fonts using backend-managed command buffers
    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        throw std::runtime_error("ImGui_ImplVulkan_CreateFontsTexture failed");
    }
}

void VulkanImGuiApp::mainLoop()
{
    bool show_demo = true;
    bool show_window = true;

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        VulkanImGuiApp::FrameSync& fs = frames_[currentFrame_];
        vkWaitForFences(device_, 1, &fs.inFlight, VK_TRUE, UINT64_MAX);
        vkResetFences(device_, 1, &fs.inFlight);

        uint32_t imageIndex = 0;
        VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, fs.imageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); continue; }
        else if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
            std::cerr << "Failed to acquire swapchain image: " << acq << std::endl; break;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (show_window) {
            ImGui::Begin("Hello, ImGui + Vulkan");
            ImGui::Text("To jest podstawowe okno ImGui.");
            ImGui::Checkbox("Pokaż Demo", &show_demo);
            if (ImGui::Button("Zamknij")) show_window = false;
            ImGui::End();
        }
        if (show_demo) ImGui::ShowDemoWindow(&show_demo);

        ImGui::Render();

        VkCommandBuffer cmd = commandBuffers_[imageIndex];
        vkResetCommandBuffer(cmd, 0);
        recordCommandBuffer(cmd, imageIndex);

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.waitSemaphoreCount = 1; si.pWaitSemaphores = &fs.imageAvailable; si.pWaitDstStageMask = waitStages;
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        si.signalSemaphoreCount = 1; si.pSignalSemaphores = &fs.renderFinished;
        checkVk(vkQueueSubmit(graphicsQueue_, 1, &si, fs.inFlight), "vkQueueSubmit failed");

        VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &fs.renderFinished;
        pi.swapchainCount = 1; pi.pSwapchains = &swapchain_; pi.pImageIndices = &imageIndex;
        VkResult pres = vkQueuePresentKHR(presentQueue_, &pi);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
        } else if (pres != VK_SUCCESS) {
            std::cerr << "vkQueuePresentKHR failed: " << pres << std::endl; break;
        }

        currentFrame_ = (currentFrame_ + 1) % static_cast<uint32_t>(frames_.size());
    }
}

void VulkanImGuiApp::cleanup()
{
    // ImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Vulkan sync
    for (auto& f : frames_) {
        if (f.imageAvailable) vkDestroySemaphore(device_, f.imageAvailable, nullptr);
        if (f.renderFinished) vkDestroySemaphore(device_, f.renderFinished, nullptr);
        if (f.inFlight) vkDestroyFence(device_, f.inFlight, nullptr);
    }

    if (imguiDescriptorPool_) vkDestroyDescriptorPool(device_, imguiDescriptorPool_, nullptr);

    if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);

    cleanupSwapchain();

    if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);

    if (device_) vkDestroyDevice(device_, nullptr);
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    destroyDebugMessenger();
    if (instance_) vkDestroyInstance(instance_, nullptr);

    if (window_) { glfwDestroyWindow(window_); window_ = nullptr; }
    glfwTerminate();
}

// ---- Vulkan helpers ----

void VulkanImGuiApp::createInstance(bool enableValidation)
{
    std::vector<const char*> layers;
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

    checkVk(vkCreateInstance(&ci, nullptr, &instance_), "vkCreateInstance failed");
}

void VulkanImGuiApp::pickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (auto d : devices) {
        if (isDeviceSuitable(d, surface_)) { physicalDevice_ = d; break; }
    }
    if (!physicalDevice_) throw std::runtime_error("Failed to find suitable GPU");
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

    checkVk(vkCreateDevice(physicalDevice_, &dci, nullptr, &device_), "vkCreateDevice failed");

    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

void VulkanImGuiApp::createSwapchain()
{
    auto support = querySwapChainSupport(physicalDevice_, surface_);
    auto surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    auto presentMode = choosePresentMode(support.presentModes);
    auto extent = chooseExtent(support.capabilities, window_);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR ci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface = surface_;
    ci.minImageCount = imageCount;
    ci.imageFormat = surfaceFormat.format;
    ci.imageColorSpace = surfaceFormat.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = findQueueFamilies(physicalDevice_, surface_);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
    if (indices.graphicsFamily != indices.presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    ci.preTransform = support.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;

    checkVk(vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_), "vkCreateSwapchain failed");

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr);
    std::vector<VkImage> images(count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &count, images.data());

    swapchainImages_.clear();
    swapchainImages_.reserve(count);
    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;

    for (auto img : images) {
        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = img;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = swapchainImageFormat_;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        ImageWithView iw{}; iw.image = img;
        checkVk(vkCreateImageView(device_, &ivci, nullptr, &iw.view), "vkCreateImageView failed");
        swapchainImages_.push_back(iw);
    }
}

void VulkanImGuiApp::createRenderPass()
{
    VkAttachmentDescription color{};
    color.format = swapchainImageFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.pDependencies = &dep;
    rpci.dependencyCount = 1;

    checkVk(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_), "vkCreateRenderPass failed");
}

void VulkanImGuiApp::createFramebuffers()
{
    framebuffers_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageView attachments[] = { swapchainImages_[i].view };
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = renderPass_;
        fbci.attachmentCount = 1;
        fbci.pAttachments = attachments;
        fbci.width = swapchainExtent_.width;
        fbci.height = swapchainExtent_.height;
        fbci.layers = 1;
        checkVk(vkCreateFramebuffer(device_, &fbci, nullptr, &framebuffers_[i]), "vkCreateFramebuffer failed");
    }
}

void VulkanImGuiApp::createCommandPoolAndBuffers()
{
    auto indices = findQueueFamilies(physicalDevice_, surface_);
    VkCommandPoolCreateInfo cpci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci.queueFamilyIndex = indices.graphicsFamily.value();
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    checkVk(vkCreateCommandPool(device_, &cpci, nullptr, &commandPool_), "vkCreateCommandPool failed");

    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = commandPool_;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    checkVk(vkAllocateCommandBuffers(device_, &cbai, commandBuffers_.data()), "vkAllocateCommandBuffers failed");
}

void VulkanImGuiApp::createSyncObjects()
{
    const size_t MAX_FRAMES_IN_FLIGHT = 2;
    frames_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& f : frames_) {
        checkVk(vkCreateSemaphore(device_, &sci, nullptr, &f.imageAvailable), "vkCreateSemaphore failed");
        checkVk(vkCreateSemaphore(device_, &sci, nullptr, &f.renderFinished), "vkCreateSemaphore failed");
        checkVk(vkCreateFence(device_, &fci, nullptr, &f.inFlight), "vkCreateFence failed");
    }
}

void VulkanImGuiApp::createDescriptorPoolForImGui()
{
    std::vector<VkDescriptorPoolSize> pool_sizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * static_cast<uint32_t>(pool_sizes.size());
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    checkVk(vkCreateDescriptorPool(device_, &pool_info, nullptr, &imguiDescriptorPool_), "vkCreateDescriptorPool failed");
}

void VulkanImGuiApp::cleanupSwapchain()
{
    for (auto fb : framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    for (auto& iw : swapchainImages_) vkDestroyImageView(device_, iw.view, nullptr);
    swapchainImages_.clear();
    if (swapchain_) { vkDestroySwapchainKHR(device_, swapchain_, nullptr); swapchain_ = VK_NULL_HANDLE; }
}

void VulkanImGuiApp::recreateSwapchain()
{
    int w = 0, h = 0;
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window_, &w, &h);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);

    cleanupSwapchain();

    createSwapchain();
    createFramebuffers();

    if (commandPool_) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    createCommandPoolAndBuffers();

    // Inform ImGui backend of new minimum image count
    ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapchainImages_.size()));
}

void VulkanImGuiApp::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    checkVk(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer failed");

    VkClearValue clear{}; clear.color = { { 0.10f, 0.15f, 0.20f, 1.0f } };

    VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpbi.renderPass = renderPass_;
    rpbi.framebuffer = framebuffers_[imageIndex];
    rpbi.renderArea.offset = {0,0};
    rpbi.renderArea.extent = swapchainExtent_;
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRenderPass(cmd);

    checkVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer failed");
}

// ---- Utility ----

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

VulkanImGuiApp::SwapChainSupportDetails VulkanImGuiApp::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails d{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &d.capabilities);
    uint32_t fmtCount = 0, pmCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmtCount, nullptr);
    if (fmtCount) { d.formats.resize(fmtCount); vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmtCount, d.formats.data()); }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &pmCount, nullptr);
    if (pmCount) { d.presentModes.resize(pmCount); vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &pmCount, d.presentModes.data()); }
    return d;
}

VkSurfaceFormatKHR VulkanImGuiApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

VkPresentModeKHR VulkanImGuiApp::choosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
    for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanImGuiApp::chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window)
{
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w, h; glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D e{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    e.width = std::clamp(e.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    e.height = std::clamp(e.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
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

void VulkanImGuiApp::setupDebugMessenger()
{
#ifndef NDEBUG
    // Load function pointers (no prototypes for extension functions by default)
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
