#pragma once

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <string>

// Forward declaration to avoid including GLFW in public header
struct GLFWwindow;

#include <cstdint>
#include <vector>
#include <optional>

class VulkanImGuiApp {
public:
    int run();
    int runSmokeTest();

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        [[nodiscard]] bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct FrameSync {
        VkSemaphore imageAvailable{};
        VkSemaphore renderFinished{};
        VkFence inFlight{};
    };

    struct ImageWithView {
        VkImage image{};
        VkImageView view{};
    };

    // Prosty sprite
    struct Sprite {
        VkImage        image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView    view  = VK_NULL_HANDLE;
        VkSampler      sampler = VK_NULL_HANDLE;
        ImTextureID    imTex = (ImTextureID)0;
        uint32_t       width = 0;
        uint32_t       height = 0;
        ImVec2         pos{0.0f, 0.0f};
        ImVec2         size{0.0f, 0.0f}; // 0 => użyj oryginalnych wymiarów
        bool           visible = true;
    };

    // State
    GLFWwindow* window_ = nullptr;

    VkInstance instance_{};
    VkSurfaceKHR surface_{};

    VkPhysicalDevice physicalDevice_{};
    VkDevice device_{};
    VkQueue graphicsQueue_{};
    VkQueue presentQueue_{};

    VkDebugUtilsMessengerEXT debugMessenger_{};

    VkSwapchainKHR swapchain_{};
    VkFormat swapchainImageFormat_{};
    VkExtent2D swapchainExtent_{};
    std::vector<ImageWithView> swapchainImages_;

    VkRenderPass renderPass_{};
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool commandPool_{};
    std::vector<VkCommandBuffer> commandBuffers_{};

    VkDescriptorPool imguiDescriptorPool_{};

    std::vector<FrameSync> frames_;
    uint32_t currentFrame_ = 0;

    // Kolekcja sprite'ów
    std::vector<Sprite> sprites_;

    // Pojedyncza tekstura (zostawiona, jeśli używasz)
    VkImage        characterImage_ = VK_NULL_HANDLE;
    VkDeviceMemory characterImageMemory_ = VK_NULL_HANDLE;
    VkImageView    characterImageView_ = VK_NULL_HANDLE;
    VkSampler      characterSampler_ = VK_NULL_HANDLE;
    ImTextureID    characterImTex_ = (ImTextureID)0;

private:
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

    // API sprite'ów
    int  addSpriteFromFile(const std::string& path);
    void setSpriteRect(int id, float x, float y, float w, float h);
    void setSpriteVisible(int id, bool visible);
    void removeSprite(int id);
    void clearSprites();
    void destroySprite(Sprite& s); // prywatne niszczenie jednego sprite'a

    // --- Tekstura postaci (stare API) ---
    void createCharacterTextureFromFile(const std::string& path);
    void destroyCharacterTexture();

    // --- Helpery Vulkan używane przy ładowaniu tekstur ---
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkImageView createImageView(VkImage image, VkFormat format);

    // Utility
    static std::vector<const char*> getRequiredExtensions(bool enableValidation);
    static bool checkValidationLayerSupport(const std::vector<const char*>& layers);
    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes);
    static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window);
    static bool wantValidationLayers();
};
