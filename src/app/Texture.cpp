#include "VulkanImGuiApp.h"
#include <vk_utils.h>
#include <stdexcept>
#include <cstring>
#include <cstdint>              // <-- DODANE
#include <imgui_impl_vulkan.h>

#if __has_include(<stb_image.h>)
  #define STB_IMAGE_IMPLEMENTATION
  #include <stb_image.h>
#elif __has_include(<stb/stb_image.h>)
  #define STB_IMAGE_IMPLEMENTATION
  #include <stb/stb_image.h>
#else
  #error "Nie znaleziono stb_image.h. Zainstaluj vcpkg 'stb' lub dodaj lokalny nag³ówek."
#endif

uint32_t VulkanImGuiApp::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

VkCommandBuffer VulkanImGuiApp::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd{};
    vkutils::checkVk(vkAllocateCommandBuffers(device_, &allocInfo, &cmd), "vkAllocateCommandBuffers failed");

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkutils::checkVk(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer failed");
    return cmd;
}

void VulkanImGuiApp::endSingleTimeCommands(VkCommandBuffer cmd)
{
    vkutils::checkVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer failed");

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkutils::checkVk(vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit failed");
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
}

void VulkanImGuiApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                  VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkutils::checkVk(vkCreateBuffer(device_, &bci, nullptr, &buffer), "vkCreateBuffer failed");

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(device_, buffer, &memReq);

    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    vkutils::checkVk(vkAllocateMemory(device_, &mai, nullptr, &bufferMemory), "vkAllocateMemory failed");
    vkutils::checkVk(vkBindBufferMemory(device_, buffer, bufferMemory, 0), "vkBindBufferMemory failed");
}

void VulkanImGuiApp::transitionImageLayout(VkImage image, VkFormat /*format*/, VkImageLayout oldLayout, VkImageLayout newLayout)
{
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

    if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // ustawione wy¿ej (TOP_OF_PIPE -> TRANSFER)
    } else {
        // Prosty fallback: ustaw jak dla finalnego u¿ycia w shaderze
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
}

void VulkanImGuiApp::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
}

VkImageView VulkanImGuiApp::createImageView(VkImage image, VkFormat format)
{
    VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;
    VkImageView view{};
    vkutils::checkVk(vkCreateImageView(device_, &ivci, nullptr, &view), "vkCreateImageView failed");
    return view;
}

void VulkanImGuiApp::createCharacterTextureFromFile(const std::string& path)
{
    // Jeœli tekstura ju¿ istnieje — najpierw usuñ
    destroyCharacterTexture();

    int texW = 0, texH = 0, texC = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &texW, &texH, &texC, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error(std::string("Failed to load image: ") + path);
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texW) * texH * 4;

    // Staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);
    void* data = nullptr;
    vkutils::checkVk(vkMapMemory(device_, stagingMemory, 0, imageSize, 0, &data), "vkMapMemory failed");
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device_, stagingMemory);

    stbi_image_free(pixels);

    // Obraz
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

    vkutils::checkVk(vkCreateImage(device_, &ici, nullptr, &characterImage_), "vkCreateImage failed");

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(device_, characterImage_, &memReq);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkutils::checkVk(vkAllocateMemory(device_, &mai, nullptr, &characterImageMemory_), "vkAllocateMemory failed");
    vkutils::checkVk(vkBindImageMemory(device_, characterImage_, characterImageMemory_, 0), "vkBindImageMemory failed");

    // Przejœcia i kopiowanie
    transitionImageLayout(characterImage_, ici.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, characterImage_, static_cast<uint32_t>(texW), static_cast<uint32_t>(texH));
    transitionImageLayout(characterImage_, ici.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // View + Sampler
    characterImageView_ = createImageView(characterImage_, ici.format);

    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.mipLodBias = 0.0f;
    sci.maxAnisotropy = 1.0f;
    sci.anisotropyEnable = VK_FALSE;
    sci.compareEnable = VK_FALSE;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    vkutils::checkVk(vkCreateSampler(device_, &sci, nullptr, &characterSampler_), "vkCreateSampler failed");

    // Rejestracja w ImGui jako ImTextureID (obs³uga wariantu, gdzie ImTextureID jest liczb¹)
    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(characterSampler_, characterImageView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    characterImTex_ = (ImTextureID)(uintptr_t)ds;

    // Sprz¹tanie staging
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void VulkanImGuiApp::destroyCharacterTexture()
{
    if (characterImTex_) {
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)(uintptr_t)characterImTex_);
        characterImTex_ = (ImTextureID)0;
    }
    if (characterSampler_) { vkDestroySampler(device_, characterSampler_, nullptr); characterSampler_ = VK_NULL_HANDLE; }
    if (characterImageView_) { vkDestroyImageView(device_, characterImageView_, nullptr); characterImageView_ = VK_NULL_HANDLE; }
    if (characterImage_) { vkDestroyImage(device_, characterImage_, nullptr); characterImage_ = VK_NULL_HANDLE; }
    if (characterImageMemory_) { vkFreeMemory(device_, characterImageMemory_, nullptr); characterImageMemory_ = VK_NULL_HANDLE; }
}

// ====== SPRITE API ======

int VulkanImGuiApp::addSpriteFromFile(const std::string& path)
{
    int texW = 0, texH = 0, texC = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &texW, &texH, &texC, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error(std::string("Failed to load image: ") + path);
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texW) * texH * 4;

    // Staging
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);
    void* data = nullptr;
    vkutils::checkVk(vkMapMemory(device_, stagingMemory, 0, imageSize, 0, &data), "vkMapMemory failed");
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device_, stagingMemory);
    stbi_image_free(pixels);

    // Image
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

    Sprite s{};
    vkutils::checkVk(vkCreateImage(device_, &ici, nullptr, &s.image), "vkCreateImage failed");

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(device_, s.image, &memReq);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkutils::checkVk(vkAllocateMemory(device_, &mai, nullptr, &s.memory), "vkAllocateMemory failed");
    vkutils::checkVk(vkBindImageMemory(device_, s.image, s.memory, 0), "vkBindImageMemory failed");

    // Copy + layout
    transitionImageLayout(s.image, ici.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, s.image, static_cast<uint32_t>(texW), static_cast<uint32_t>(texH));
    transitionImageLayout(s.image, ici.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // View + Sampler
    s.view = createImageView(s.image, ici.format);

    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.mipLodBias = 0.0f;
    sci.maxAnisotropy = 1.0f;
    sci.anisotropyEnable = VK_FALSE;
    sci.compareEnable = VK_FALSE;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    vkutils::checkVk(vkCreateSampler(device_, &sci, nullptr, &s.sampler), "vkCreateSampler failed");

    // Rejestracja w ImGui
    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(s.sampler, s.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s.imTex = (ImTextureID)(uintptr_t)ds;

    // Wymiary i domyœlny rozmiar
    s.width = static_cast<uint32_t>(texW);
    s.height = static_cast<uint32_t>(texH);
    s.size = ImVec2((float)s.width, (float)s.height);

    // Sprz¹tanie staging
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);

    sprites_.push_back(s);
    return static_cast<int>(sprites_.size() - 1);
}

void VulkanImGuiApp::setSpriteRect(int id, float x, float y, float w, float h)
{
    if (id < 0 || (size_t)id >= sprites_.size()) return;
    sprites_[id].pos = ImVec2(x, y);
    sprites_[id].size = ImVec2(w > 0 ? w : (float)sprites_[id].width,
                               h > 0 ? h : (float)sprites_[id].height);
}

void VulkanImGuiApp::setSpriteVisible(int id, bool visible)
{
    if (id < 0 || (size_t)id >= sprites_.size()) return;
    sprites_[id].visible = visible;
}

void VulkanImGuiApp::destroySprite(Sprite& s)
{
    if (s.imTex) ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)(uintptr_t)s.imTex);
    if (s.sampler) vkDestroySampler(device_, s.sampler, nullptr);
    if (s.view) vkDestroyImageView(device_, s.view, nullptr);
    if (s.image) vkDestroyImage(device_, s.image, nullptr);
    if (s.memory) vkFreeMemory(device_, s.memory, nullptr);
    s = Sprite{};
}

void VulkanImGuiApp::removeSprite(int id)
{
    if (id < 0 || (size_t)id >= sprites_.size()) return;
    destroySprite(sprites_[id]); // zostawiamy „dziurê” dla stabilnych ID
}

void VulkanImGuiApp::clearSprites()
{
    for (auto& s : sprites_) destroySprite(s);
    sprites_.clear();
}