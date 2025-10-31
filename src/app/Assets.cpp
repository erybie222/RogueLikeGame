#include "Assets.h"
#include <imgui_impl_vulkan.h>
#include <stdexcept>
#include <cstring>

#if __has_include(<stb_image.h>)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#elif __has_include(<stb/stb_image.h>)
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#else
#error "Nie znaleziono stb_image.h. Zainstaluj vcpkg 'stb' lub dodaj lokalny nag³ówek."
#endif

Assets::Assets(const Ctx& ctx) : ctx_(ctx) {}
Assets::~Assets() { clear(); }

uint32_t Assets::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(ctx_.physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Assets::findMemoryType: no suitable memory type");
}

VkCommandBuffer Assets::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = ctx_.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd{};
    vkAllocateCommandBuffers(ctx_.device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void Assets::endSingleTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx_.graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_.graphicsQueue);
    vkFreeCommandBuffers(ctx_.device, ctx_.commandPool, 1, &cmd);
}

void Assets::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer& buffer, VkDeviceMemory& bufferMemory) const {
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(ctx_.device, &bci, nullptr, &buffer);

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(ctx_.device, buffer, &memReq);

    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    vkAllocateMemory(ctx_.device, &mai, nullptr, &bufferMemory);
    vkBindBufferMemory(ctx_.device, buffer, bufferMemory, 0);
}

void Assets::transitionImageLayout(VkImage image, VkFormat /*format*/,
    VkImageLayout oldLayout, VkImageLayout newLayout) const {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // default OK
    }
    else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
}

void Assets::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
}

VkImageView Assets::createImageView(VkImage image, VkFormat format) const {
    VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;

    VkImageView view{};
    vkCreateImageView(ctx_.device, &ivci, nullptr, &view);
    return view;
}

SpriteId Assets::addSpriteFromFile(const std::string& path) {
    int texW = 0, texH = 0, texC = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &texW, &texH, &texC, STBI_rgb_alpha);
    if (!pixels) throw std::runtime_error("Failed to load image: " + path);

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texW) * texH * 4;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* data = nullptr;
    vkMapMemory(ctx_.device, stagingMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(ctx_.device, stagingMemory);
    stbi_image_free(pixels);

    SpriteGPU s{};

    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent = { static_cast<uint32_t>(texW), static_cast<uint32_t>(texH), 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(ctx_.device, &ici, nullptr, &s.image);

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(ctx_.device, s.image, &memReq);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(ctx_.device, &mai, nullptr, &s.memory);
    vkBindImageMemory(ctx_.device, s.image, s.memory, 0);

    transitionImageLayout(s.image, ici.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, s.image, static_cast<uint32_t>(texW), static_cast<uint32_t>(texH));
    transitionImageLayout(s.image, ici.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    s.view = createImageView(s.image, ici.format);

    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxAnisotropy = 1.0f;
    sci.anisotropyEnable = VK_FALSE;
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;

    vkCreateSampler(ctx_.device, &sci, nullptr, &s.sampler);

    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(s.sampler, s.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s.imTex = (ImTextureID)(uintptr_t)ds;

    s.width = static_cast<uint32_t>(texW);
    s.height = static_cast<uint32_t>(texH);

    vkDestroyBuffer(ctx_.device, stagingBuffer, nullptr);
    vkFreeMemory(ctx_.device, stagingMemory, nullptr);

    sprites_.push_back(s);
    return static_cast<int>(sprites_.size() - 1);
}

void Assets::destroySprite(const Ctx& ctx, SpriteGPU& s) {
    if (s.imTex) ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)(uintptr_t)s.imTex);
    if (s.sampler) vkDestroySampler(ctx.device, s.sampler, nullptr);
    if (s.view)    vkDestroyImageView(ctx.device, s.view, nullptr);
    if (s.image)   vkDestroyImage(ctx.device, s.image, nullptr);
    if (s.memory)  vkFreeMemory(ctx.device, s.memory, nullptr);
    s = SpriteGPU{};
}

void Assets::removeSprite(SpriteId id) {
    if (id < 0 || (size_t)id >= sprites_.size()) return;
    destroySprite(ctx_, sprites_[id]);
}

void Assets::clear() {
    for (auto& s : sprites_) destroySprite(ctx_, s);
    sprites_.clear();
}
