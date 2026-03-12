#pragma once
#include "engine/gfx/Assets.h"
#include "game/world/World.h"

#include <imgui.h>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;
class Player;
class AiServerApp;

#include <cstdint>
#include <optional>
#include <vector>

class VulkanImGuiApp
{
public:
    VulkanImGuiApp();
    ~VulkanImGuiApp();
    int run();
    int runSmokeTest();
    void enableAiMode();

private:
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        [[nodiscard]] bool      isComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    struct FrameSync
    {
        VkSemaphore imageAvailable{};
        VkSemaphore renderFinished{};
        VkFence     inFlight{};
    };

    struct ImageWithView
    {
        VkImage     image{};
        VkImageView view{};
    };

    // State
    GLFWwindow* window_ = nullptr;

    VkInstance   instance_{};
    VkSurfaceKHR surface_{};

    VkPhysicalDevice physicalDevice_{};
    VkDevice         device_{};
    VkQueue          graphicsQueue_{};
    VkQueue          presentQueue_{};

    VkDebugUtilsMessengerEXT debugMessenger_{};

    VkSwapchainKHR             swapchain_{};
    VkFormat                   swapchainImageFormat_{};
    VkExtent2D                 swapchainExtent_{};
    std::vector<ImageWithView> swapchainImages_;

    VkRenderPass               renderPass_{};
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool                commandPool_{};
    std::vector<VkCommandBuffer> commandBuffers_{};

    VkDescriptorPool imguiDescriptorPool_{};

    std::vector<FrameSync> frames_;
    uint32_t               currentFrame_ = 0;

    std::unique_ptr<Assets> assets_ = nullptr;
    std::unique_ptr<World>  world_;
    std::unique_ptr<AiServerApp> aiServer_;
    bool                    aiMode_ = false;
    IconId                  heartIconId_ = -1;
    IconId                  bowIconId_ = -1;
    IconId                  swordIconId_ = -1;
    bool                    isPaused_ = false;
    bool                    resourcesBeingUpdated_ = false;
    float const             slotSize = 45.0f;
    float const             padding = 4.0f;

    // High-level steps
    void initWindow();
    void initVulkan();
    void initImGui();
    void mainLoop();
    void cleanup();

    // Vulkan helpers
    void createInstance();
    void setupDebugMessenger();
    void destroyDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPoolAndBuffers();
    void createSyncObjects();
    void createDescriptorPoolForImGui();
    void cleanupSwapchain();
    void recreateSwapchain();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void reinitImGuiRenderer();

    // Rysowanie świata
    void drawWorld();
    void drawInventoryUI();
    void drawHeartsUI(ImDrawList* bg, Player* player);
    void drawPauseMenu();
    void drawDeathView();
    void drawWinView();
    void drawAttackMode(AttackMode attack_mode);

    // --- Helpery Vulkan używane przy ładowaniu tekstur ---
    uint32_t        findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer beginSingleTimeCommands();
    void            endSingleTimeCommands(VkCommandBuffer cmd);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer,
                      VkDeviceMemory& bufferMemory);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkImageView createImageView(VkImage image, VkFormat format);

    // Utility
    static std::vector<const char*> getRequiredExtensions(bool enableValidation);
    static bool                     checkValidationLayerSupport(const std::vector<const char*>& layers);
    static QueueFamilyIndices       findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    static bool                     isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    static SwapChainSupportDetails  querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    static VkSurfaceFormatKHR       chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    static VkPresentModeKHR         choosePresentMode(const std::vector<VkPresentModeKHR>& modes);
    static VkExtent2D               chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window);
    static bool                     wantValidationLayers();
};
