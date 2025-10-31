#pragma once
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

using SpriteId = int;

// GPU-strona sprite'a (bez pozycji/widocznoœci – to jest logika gry)
struct SpriteGPU {
    VkImage        image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView    view = VK_NULL_HANDLE;
    VkSampler      sampler = VK_NULL_HANDLE;
    ImTextureID    imTex = (ImTextureID)0;
    uint32_t       width = 0;
    uint32_t       height = 0;
};

class Assets {
public:
    struct Ctx {
        VkPhysicalDevice physicalDevice{};
        VkDevice device{};
        VkQueue graphicsQueue{};
        VkCommandPool commandPool{};
    };

    explicit Assets(const Ctx& ctx);
    ~Assets();

    SpriteId addSpriteFromFile(const std::string& path);
    SpriteId getOrLoad(const std::string& path);
    const SpriteGPU& sprite(SpriteId id) const { return sprites_[id]; }

    void removeSprite(SpriteId id);   // zostawia „dziurê” – stabilne ID
    void clear();                     // czyœci wszystko

private:
    Ctx ctx_;
    std::vector<SpriteGPU> sprites_;

    std::unordered_map<std::string, SpriteId> byPath_;
    std::vector < std::string> paths_;

    // Pomocnicze (przeniesione z Texture.cpp)
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer cmd) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    VkImageView createImageView(VkImage image, VkFormat format) const;

    static void destroySprite(const Ctx& ctx, SpriteGPU& s);
};
