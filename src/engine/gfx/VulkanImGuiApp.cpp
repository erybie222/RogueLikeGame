#include "VulkanImGuiApp.h"
#include "../../app/classes/Entity.h"
#include "GameSetup.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vk_utils.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "Player.h"
#include "Map.h"
#include <memory>
#include "World.h"
#include <cmath>

int VulkanImGuiApp::run()
{
    try {
        initWindow();
        initVulkan();
        initImGui();
        // --- Wczytaj ikone jako teksture i zarejestruj w ImGui ---
		world_ = std::make_unique<World>(assets_.get());
		setupGame(*world_);
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
    //tymczasowo tu zeby bylo widac ale kiedys do refaktoryzaji
    Assets::Ctx actx{ physicalDevice_, device_, graphicsQueue_, commandPool_ };
    assets_ = std::make_unique<Assets>(actx);
}

static void drawDebugOverlay(GLFWwindow* window, const std::vector<std::unique_ptr<Entity>>& entityList, const Player* mainPlayer){
    // Zbierz dane
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);

    auto& io = ImGui::GetIO();

    // Ustawienia okienka (przyklejone w lewym górnym rogu, półprzezroczyste)
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Debug overlay", nullptr, flags))
    {
        ImGui::Text("FPS: %.1f  (dt=%.4f s)", io.Framerate, io.DeltaTime);
        ImGui::Separator();

        ImGui::Text("GLFW Window:      %d x %d", winW, winH);
        ImGui::Text("GLFW Framebuffer: %d x %d", fbW, fbH);
        ImGui::Text("ImGui DisplaySize: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("ImGui FB Scale:   %.2f x %.2f", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

        ImGui::Separator();
        ImGui::Text("Entities: %d", (int)entityList.size());
        if (ImGui::TreeNode("List"))
        {
            for (size_t i =0; i < entityList.size(); ++i)
            {
                if (!entityList[i]) continue;
                ImVec2 p = entityList[i]->getPosition();
                ImGui::BulletText("#%zu pos=(%.0f, %.0f) size=%ux%u entityId=%d",
                    i, p.x, p.y, entityList[i]->getWidth(), entityList[i]->getHeight(), entityList[i]->getEntityId());
            }
            ImGui::TreePop();
        }

        if (mainPlayer)
        {
            ImGui::Separator();
            ImVec2 pp = mainPlayer->getPosition();
            ImGui::Text("Player: pos=(%.0f, %.0f) size=%ux%u entityId=%d",
                pp.x, pp.y, mainPlayer->getWidth(), mainPlayer->getHeight(), mainPlayer->getEntityId());
        }
    }
    ImGui::End();
}

void VulkanImGuiApp::mainLoop()
{
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Sprawdz ESC najpierw, zanim ruszymy sync (unikanie potencjalnego czekania na fence po wyjściu)
        if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
            break; // wyjdz od razu
        }

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

   float dt = ImGui::GetIO().DeltaTime;

        // Update screen bounds for world
      if (world_) {
   auto& io = ImGui::GetIO();
  world_->setScreenBounds(io.DisplaySize.x, io.DisplaySize.y);
        }

      // --- Proste sterowanie WASD oparte o GLFW (nie zależne od stanu przechwycenia klawiatury przez ImGui) ---
      if (auto* player = world_ ? world_->getPlayer() : nullptr) {
        float dx = 0.0f, dy = 0.0f;
            if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) dy -= 1.0f;
            if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) dy += 1.0f;
            if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) dx -= 1.0f;
      if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) dx += 1.0f;
         player->applyInput(ImVec2(dx,dy));
   }

		if (world_) world_->update(dt);


        // --- Rysowanie swiata/tla (poza oknami) ---
        drawWorld();

        //debug window
        if (world_) drawDebugOverlay(window_, world_->entities(), world_->getPlayer());

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
    if (device_) vkDeviceWaitIdle(device_);

    world_.reset();
    // ImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    assets_.reset();
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

void VulkanImGuiApp::drawWorld()
{
    if (!assets_ || !world_) return;

    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    for (const auto& up : world_->entities()) {
        if (!up) continue;
        if (up.get() == world_->getPlayer()) continue;

        const Entity& e = *up;
        if (!e.isVisible()) continue;

        const ImVec2 pos = e.getPosition();
        const uint32_t w = e.getWidth();
        const uint32_t h = e.getHeight();

        const auto& entity = assets_->icon(e.getEntityId());
        bg->AddImage(
            entity.imTex,
            pos,
            ImVec2(pos.x + static_cast<float>(w), pos.y + static_cast<float>(h)),
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32_WHITE
        );
    }

    if (auto* player = world_->getPlayer()) {
        const ImVec2 pos = player->getPosition();
        const uint32_t w = player->getWidth();
        const uint32_t h = player->getHeight();

        const auto& entity = assets_->icon(player->getEntityId());
        bg->AddImage(
            entity.imTex,
            pos,
            ImVec2(pos.x + static_cast<float>(w), pos.y + static_cast<float>(h)),
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32_WHITE
        );
    }
}

