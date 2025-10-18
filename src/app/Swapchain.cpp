#include "VulkanImGuiApp.h"
#include <GLFW/glfw3.h>
#include <vk_utils.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <algorithm>

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

    vkutils::checkVk(vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_), "vkCreateSwapchain failed");

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
        vkutils::checkVk(vkCreateImageView(device_, &ivci, nullptr, &iw.view), "vkCreateImageView failed");
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

    vkutils::checkVk(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_), "vkCreateRenderPass failed");
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
        vkutils::checkVk(vkCreateFramebuffer(device_, &fbci, nullptr, &framebuffers_[i]), "vkCreateFramebuffer failed");
    }
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

    if (renderPass_) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    // Recreate swapchain and render pass
    createSwapchain();
    createRenderPass();
    createFramebuffers();

    if (commandPool_) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    createCommandPoolAndBuffers();

    // Re-init ImGui Vulkan backend with the new render pass and image counts
    reinitImGuiRenderer();

    ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapchainImages_.size()));
}

void VulkanImGuiApp::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkutils::checkVk(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer failed");

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

    vkutils::checkVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer failed");
}
