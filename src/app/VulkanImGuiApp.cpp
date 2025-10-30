#include "VulkanImGuiApp.h"
#include "game.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vk_utils.h>
#include <iostream>
#include <stdexcept>

Entity* player_ = nullptr;

int VulkanImGuiApp::run()
{
    try {
        initWindow();
        initVulkan();
        initImGui();
        // --- Wczytaj ikonę jako teksturę i zarejestruj w ImGui ---
        int playerId = addSpriteFromFile("assets/characters/hero.png");
        player_ = new Entity(playerId, 64, 64, 256.0f, 256.0f); //  width, height, x, y

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

void VulkanImGuiApp::initVulkan()
{
    createInstance();
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

        // --- Rysowanie świata/tła (poza oknami) ---
        drawWorld();

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
        vkutils::checkVk(vkQueueSubmit(graphicsQueue_, 1, &si, fs.inFlight), "vkQueueSubmit failed");

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
    // Najpierw sprite'y (mają ImGui descriptor sets)
    clearSprites();

    // (opcjonalnie) stara pojedyncza tekstura
    destroyCharacterTexture();

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

// --- Rysowanie tła i innych obiektów (poza oknami ImGui) ---
void VulkanImGuiApp::drawWorld()
{
    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    ImVec2 pos = player_->getPosition();
    uint32_t width = player_->getWidth();
    uint32_t height = player_->getHeight();
    int spriteId = player_->getSpriteId();

    Sprite& sprite = sprites_[spriteId];

    bg->AddImage(sprite.imTex, pos, ImVec2(pos.x + width, pos.y + height),
                    ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
    

    // (opcjonalnie) stara pełnoekranowa tekstura:
    // if (characterImTex_) { ... }
}
