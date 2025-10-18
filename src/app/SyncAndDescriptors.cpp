#include "VulkanImGuiApp.h"
#include <vk_utils.h>

void VulkanImGuiApp::createCommandPoolAndBuffers()
{
    auto indices = findQueueFamilies(physicalDevice_, surface_);
    VkCommandPoolCreateInfo cpci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci.queueFamilyIndex = indices.graphicsFamily.value();
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkutils::checkVk(vkCreateCommandPool(device_, &cpci, nullptr, &commandPool_), "vkCreateCommandPool failed");

    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = commandPool_;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    vkutils::checkVk(vkAllocateCommandBuffers(device_, &cbai, commandBuffers_.data()), "vkAllocateCommandBuffers failed");
}

void VulkanImGuiApp::createSyncObjects()
{
    const size_t MAX_FRAMES_IN_FLIGHT = 2;
    frames_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& f : frames_) {
        vkutils::checkVk(vkCreateSemaphore(device_, &sci, nullptr, &f.imageAvailable), "vkCreateSemaphore failed");
        vkutils::checkVk(vkCreateSemaphore(device_, &sci, nullptr, &f.renderFinished), "vkCreateSemaphore failed");
        vkutils::checkVk(vkCreateFence(device_, &fci, nullptr, &f.inFlight), "vkCreateFence failed");
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
    vkutils::checkVk(vkCreateDescriptorPool(device_, &pool_info, nullptr, &imguiDescriptorPool_), "vkCreateDescriptorPool failed");
}

