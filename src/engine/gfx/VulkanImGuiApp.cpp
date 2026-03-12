#include "VulkanImGuiApp.h"

#include "game/entities/Entity.h"
#include "game/entities/LivingEntity.h"
#include "game/entities/animation/AnimationController.h"
#include "app/GameSetup.h"
#include "game/world/Map.h"
#include "game/entities/Player.h"
#include "game/world/World.h"
#include "game/item/Inventory.h"
#include "game/item/Item.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <vk_utils.h>
#include "game/factory/ItemFactory.h"
#include "utils/MathUtils.h"
#include "app/AiServerApp.h"

VulkanImGuiApp::VulkanImGuiApp() = default;
VulkanImGuiApp::~VulkanImGuiApp() = default;

int VulkanImGuiApp::run()
{
    try
    {
        initWindow();
        initVulkan();
        initImGui();

        heartIconId_ = assets_->getOrLoadIcon("assets/design/heart.png");

        if (assets_) {
            try {
                bowIconId_ = assets_->getOrLoadIcon("assets/design/bow.png");
                swordIconId_ = assets_->getOrLoadIcon("assets/design/sword.png");
            } catch (const std::exception& e) {
                std::cerr << "Warning: failed to load UI icons: " << e.what() << std::endl;
                bowIconId_ = -1;
                swordIconId_ = -1;
            }
        }

        world_ = std::make_unique<World>(assets_.get());
        setupGame(*world_);

        if (aiMode_ && aiServer_)
        {
            aiServer_->start();
        }

        mainLoop();
        vkDeviceWaitIdle(device_);

        if (aiServer_)
            aiServer_->stop();

        cleanup();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        if (aiServer_)
            aiServer_->stop();
        cleanup();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int VulkanImGuiApp::runSmokeTest()
{
    try
    {
        std::cout << "[Smoke] Init window..." << std::endl;
        initWindow();
        std::cout << "[Smoke] Init Vulkan..." << std::endl;
        initVulkan();
        std::cout << "[Smoke] Init ImGui..." << std::endl;
        initImGui();
        vkDeviceWaitIdle(device_);
        std::cout << "[Smoke] OK" << std::endl;
        cleanup();
    }
    catch (const std::exception& e)
    {
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
    // tymczasowo tu zeby bylo widac ale kiedys do refaktoryzaji
    Assets::Ctx actx{physicalDevice_, device_, graphicsQueue_, commandPool_};
    assets_ = std::make_unique<Assets>(actx);
}

static void drawDebugOverlay(GLFWwindow* window, const std::vector<std::unique_ptr<Entity>>& entityList,
                             const Player* mainPlayer, const World* world)
{
    // Zbierz dane
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);

    auto& io = ImGui::GetIO();

    // Ustawienia okienka (przyklejone w lewym górnym rogu, półprzezroczyste)
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

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
            for (size_t i = 0; i < entityList.size(); ++i)
            {
                if (!entityList[i])
                    continue;
                ImVec2 p      = entityList[i]->getPosition();
                auto*  living = dynamic_cast<LivingEntity*>(entityList[i].get());
                if (living)
                {
                    ImGui::BulletText("#%zu pos=(%.0f, %.0f) size=%ux%u id=%d hp=%d/%d", i, p.x, p.y,
                                      entityList[i]->getWidth(), entityList[i]->getHeight(),
                                      entityList[i]->getEntityId(), living->getHp(), living->getMaxHp());
                }
                else
                {
                    ImGui::BulletText("#%zu pos=(%.0f, %.0f) size=%ux%u id=%d", i, p.x, p.y, entityList[i]->getWidth(),
                                      entityList[i]->getHeight(), entityList[i]->getEntityId());
                }
            }
            ImGui::TreePop();
        }

        if (mainPlayer)
        {
            ImGui::Separator();
            ImVec2 pp = mainPlayer->getPosition();
            ImGui::Text("Player: pos=(%.0f, %.0f) size=%ux%u entityId=%d", pp.x, pp.y, mainPlayer->getWidth(),
                        mainPlayer->getHeight(), mainPlayer->getEntityId());
            auto* lp = static_cast<const LivingEntity*>(mainPlayer);
            ImGui::Text("Player HP: %d / %d", lp->getHp(), lp->getMaxHp());
        }

        // Show current map index
        if (world)
        {
            ImGui::Text("Map: %d", world->getCurrentMapIndex());
        }
    }
    ImGui::End();
}

void VulkanImGuiApp::mainLoop()
{
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();

        static bool lastEsc = false;
        bool escPressed = glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escPressed && !lastEsc)
        {
            isPaused_ = !isPaused_;
        }
        lastEsc = escPressed;

        VulkanImGuiApp::FrameSync& fs = frames_[currentFrame_];
        vkWaitForFences(device_, 1, &fs.inFlight, VK_TRUE, UINT64_MAX);
        vkResetFences(device_, 1, &fs.inFlight);

        uint32_t imageIndex = 0;
        VkResult acq =
            vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, fs.imageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain();
            continue;
        }
        else if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR)
        {
            std::cerr << "Failed to acquire swapchain image: " << acq << std::endl;
            break;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        float dt = ImGui::GetIO().DeltaTime;

        // Update screen bounds for world
        if (world_)
        {
            auto& io = ImGui::GetIO();
            world_->setScreenBounds(io.DisplaySize.x, io.DisplaySize.y);
        }
        Player* player = world_->getPlayer();
        AttackMode attack_mode = player->getAttackMode();

        if (!player->isAlive() && aiMode_ && aiServer_)
        {
            restartGame(*world_);
        }
        else if (!player->isAlive()) drawDeathView();

        if (world_->isGameWon() && aiMode_ && aiServer_)
        {
            restartGame(*world_);
        }
        else if (world_->isGameWon())
        {
            drawWinView();
        }

        if (!isPaused_ && world_ && player->isAlive() && !world_->isGameWon())
        {
        if (player)
        {
            float dx = 0.0f, dy = 0.0f;

            if (aiMode_ && aiServer_)
            {
                int dir = aiServer_->consumeDirection();
                switch (dir)
                {
                    case 0: dy = -1.0f; break; // up
                    case 1: dy =  1.0f; break; // down
                    case 2: dx = -1.0f; break; // left
                    case 3: dx =  1.0f; break; // right
                }
            }
            else
            {
                if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
                    dy -= 1.0f;
                if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
                    dy += 1.0f;
                if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
                    dx -= 1.0f;
                if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
                    dx += 1.0f;
            }
            player->applyInput(ImVec2(dx, dy));

            // change attack mode
            static bool lastQ    = false;
            bool        qPressed = glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS;
            if (qPressed && !lastQ)
            {
                player->toggleAttackMode();
                attack_mode = player->getAttackMode();
            }
            lastQ = qPressed;

            static bool lastNumKeys[10] = {false};
            int keyMap[10] = {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5,
                              GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9, GLFW_KEY_0};
            for (int i = 0; i < 10; ++i)
            {
                bool pressed = glfwGetKey(window_, keyMap[i]) == GLFW_PRESS;
                if (pressed && !lastNumKeys[i])
                {
                    player->getInventory().useItem(i, *player);
                }
                lastNumKeys[i] = pressed;
            }

            // player attack
            if (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS)
            {
                if (player->isMeleeMode())
                {
                    if (player->canMelee())
                    {
                        if (!player->getAnimationController())
                        {
                            world_->performMeleeAttack(*player);
                        }
                        else
                        {
                            player->setIsPerformingMeleeAttack(true);
                        }
                        player->startMeleeCooldown();
                    }
                }
                else if (player->isRangedMode())
                {
                    if (player->canShoot())
                    {
                        if (!player->getAnimationController())
                        {
                            ImVec2 mousePos = ImGui::GetIO().MousePos;
                            ImVec2 aimDir = normalize({mousePos.x - player->getPosition().x, mousePos.y - player->getPosition().y});
                            world_->performRangedAttack(*player, aimDir);
                        }
                        else
                        {
                            player->setAimLock(0.5f);
                            player->setIsPerformingRangedAttack(true);
                        }
                        player->startRangedCooldown();
                    }
                }
            }
            static bool lastE    = false;
            bool        ePressed = glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS;
            if (ePressed && !lastE)
            {
                auto* map = world_->getMap(world_->getCurrentMapLevel(), world_->getCurrentMapIndex());
                if (map && !map->isChestOpened())
                {
                    ImVec2 p  = player->getPosition();
                    float  pw = static_cast<float>(player->getWidth());
                    float  ph = static_cast<float>(player->getHeight());

                    ChestInfo info = map->getChestInfo();
                    if (p.x + pw > info.posX && p.x < info.posX + info.width && p.y + ph > info.posY &&
                        p.y < info.posY + info.height)
                    {
                        map->setChestOpened(true);
                    }  
                }
                if (map && map->isLockedDoors())
                {
                    ImVec2 p  = player->getPosition();
                    auto& inventory = player->getInventory();
                    float  pw = static_cast<float>(player->getWidth());
                    float  ph = static_cast<float>(player->getHeight());

                    LockedDoorInfo info = map->getLockedDoorInfo();
                    if (p.x + pw > info.posX && p.x < info.posX + info.width && p.y + ph > info.posY &&
                        p.y < info.posY + info.height)
                    {
                        for (int i = 0; i < inventory.getItemCount(); i++)
                        {
                            Item* item = inventory.getItem(i);
                            if (item && item->getName() == "Key")
                            {
                                inventory.removeItem(i);
                                map->setLockedDoors(false); // for testing purposes
                            }
                        }
                    }
                }
            }
            lastE = ePressed;
        }
        }

        if (!isPaused_ && world_ && player->isAlive() && !world_->isGameWon())
        {
            world_->update(dt);
            if (aiMode_ && aiServer_)
            {
                ImVec2 pPos = player->getPosition();
                int tileX = static_cast<int>(pPos.x / 64.0f);
                int tileY = static_cast<int>((pPos.y - World::UI_TOP_BAR_HEIGHT) / 64.0f);
                aiServer_->updateResponse(player->getHp(), player->isAlive(), tileX, tileY, world_->getTileGrid());
            }
        }


        // --- Rysowanie swiata/tla (poza oknami) ---
        drawWorld();
        drawAttackMode(attack_mode);
        if (isPaused_)
            drawPauseMenu();
        // debug window
        if (world_)
            drawDebugOverlay(window_, world_->entities(), world_->getPlayer(), world_.get());

        ImGui::Render();

        // Validate imageIndex and corresponding command buffer before use
        if (imageIndex >= commandBuffers_.size())
        {
            std::cerr << "[Vulkan-ERROR] imageIndex (" << imageIndex << ") >= commandBuffers_.size() (" << commandBuffers_.size()
                      << ") - recreating swapchain and skipping this frame" << std::endl;
            recreateSwapchain();
            continue;
        }

        VkCommandBuffer cmd = commandBuffers_[imageIndex];
        if (cmd == VK_NULL_HANDLE)
        {
            std::cerr << "[Vulkan-ERROR] commandBuffers_[" << imageIndex << "] is VK_NULL_HANDLE - recreating swapchain and skipping frame" << std::endl;
            recreateSwapchain();
            continue;
        }

        VkResult resetRes = vkResetCommandBuffer(cmd, 0);
        if (resetRes != VK_SUCCESS)
        {
            std::cerr << "[Vulkan-ERROR] vkResetCommandBuffer failed: " << resetRes << " for cmd=" << cmd << " imageIndex=" << imageIndex << std::endl;
            recreateSwapchain();
            continue;
        }
        recordCommandBuffer(cmd, imageIndex);

        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo         si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.waitSemaphoreCount   = 1;
        si.pWaitSemaphores      = &fs.imageAvailable;
        si.pWaitDstStageMask    = waitStages;
        si.commandBufferCount   = 1;
        si.pCommandBuffers      = &cmd;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores    = &fs.renderFinished;
        vkutils::checkVk(vkQueueSubmit(graphicsQueue_, 1, &si, fs.inFlight), "vkQueueSubmit failed");

        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores    = &fs.renderFinished;
        pi.swapchainCount     = 1;
        pi.pSwapchains        = &swapchain_;
        pi.pImageIndices      = &imageIndex;
        VkResult pres         = vkQueuePresentKHR(presentQueue_, &pi);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapchain();
        }
        else if (pres != VK_SUCCESS)
        {
            std::cerr << "vkQueuePresentKHR failed: " << pres << std::endl;
            break;
        }

        currentFrame_ = (currentFrame_ + 1) % static_cast<uint32_t>(frames_.size());
    }
}

void VulkanImGuiApp::cleanup()
{
    if (device_)
        vkDeviceWaitIdle(device_);

    world_.reset();
    // ImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    assets_.reset();
    // Vulkan sync
    for (auto& f : frames_)
    {
        if (f.imageAvailable)
            vkDestroySemaphore(device_, f.imageAvailable, nullptr);
        if (f.renderFinished)
            vkDestroySemaphore(device_, f.renderFinished, nullptr);
        if (f.inFlight)
            vkDestroyFence(device_, f.inFlight, nullptr);
    }

    if (imguiDescriptorPool_)
        vkDestroyDescriptorPool(device_, imguiDescriptorPool_, nullptr);

    if (commandPool_)
        vkDestroyCommandPool(device_, commandPool_, nullptr);

    cleanupSwapchain();

    if (renderPass_)
        vkDestroyRenderPass(device_, renderPass_, nullptr);

    if (device_)
        vkDestroyDevice(device_, nullptr);
    if (surface_)
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    destroyDebugMessenger();
    if (instance_)
        vkDestroyInstance(instance_, nullptr);

    if (window_)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void VulkanImGuiApp::drawWorld()
{
    if (!assets_ || !world_)
        return;

    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    // Draw black bar at top for UI
    bg->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(ImGui::GetIO().DisplaySize.x, World::UI_TOP_BAR_HEIGHT),
                      IM_COL32(0, 0, 0, 255));

    auto drawHpBar = [bg](const LivingEntity& le, float x, float y, float w, float spriteScale = 1.0f)
    {
        int hp    = le.getHp();
      int maxHp = le.getMaxHp();
        if (maxHp <= 0)
            return;
        float ratio           = static_cast<float>(hp) / static_cast<float>(maxHp);
    ratio        = std::clamp(ratio, 0.0f, 1.0f);
        const float barHeight = 6.0f;
        
  // Dodatkowe przesunięcie w górę: (skala - 1) * wysokość tile'a
        float offsetY = (spriteScale - 1.0f) * 64.0f;
        
        ImVec2      barMin(x, y - offsetY - barHeight - 4.0f);
        ImVec2      barMax(x + w, y - offsetY - 4.0f);
  bg->AddRectFilled(barMin, barMax, IM_COL32(30, 30, 30, 200), 2.0f);
        bg->AddRect(barMin, barMax, IM_COL32(200, 200, 200, 255), 2.0f);
        ImVec2 hpFill(barMin.x + (barMax.x - barMin.x) * ratio, barMax.y);
        ImU32  col = (ratio > 0.5f) ? IM_COL32(0, 200, 0, 220) : IM_COL32(200, 50, 0, 220);
        bg->AddRectFilled(ImVec2(barMin.x + 1, barMin.y + 1), ImVec2(hpFill.x - 1, barMax.y - 1), col, 2.0f);
        // Tekst HP (środek paska)
 char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", hp, maxHp);
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        ImVec2 textPos(barMin.x + (barMax.x - barMin.x - textSize.x) * 0.5f, barMin.y - textSize.y - 1.0f);
        bg->AddText(textPos, IM_COL32(255, 255, 255, 220), buf);
    };

    // Helper function to draw scaled sprite centered on position
    // texX, texY - lewy górny róg w spritesheet (piksele), 0 = pełna tekstura
    auto drawScaledSprite = [bg, this](int iconId, ImVec2 pos, float width, float height, float scale,
                                        uint32_t texX = 0, uint32_t texY = 0,
                                        bool flipH = false, bool flipV = false, bool flipD = false)
    {
        const auto& icon = assets_->icon(iconId);
        if (!icon.imTex)
        {
            return;
        }

        const float renderW = width * scale;
        const float renderH = height * scale;

        // Center the larger sprite on the hitbox
        const float offsetX = (renderW - width) * 0.5f;
        const float offsetY = (renderH - height) * 0.5f;

        ImVec2 renderPos(pos.x - offsetX, pos.y - offsetY);

        // Oblicz UV na podstawie texX, texY i rozmiaru tekstury
        ImVec2 uv0(0.0f, 0.0f);
        ImVec2 uv1(1.0f, 1.0f);
        
        if (texX > 0 || texY > 0)
        {
            // Używamy spritesheet - oblicz UV
            float texWidth  = static_cast<float>(icon.width);
            float texHeight = static_cast<float>(icon.height);
            uv0 = ImVec2(static_cast<float>(texX) / texWidth, 
                         static_cast<float>(texY) / texHeight);
            uv1 = ImVec2(static_cast<float>(texX + static_cast<uint32_t>(width)) / texWidth,
                         static_cast<float>(texY + static_cast<uint32_t>(height)) / texHeight);
        }


        if (flipD)
        {
            ImVec2 uvTL = uv0;
            ImVec2 uvTR = ImVec2(uv1.x, uv0.y);
            ImVec2 uvBR = uv1;
            ImVec2 uvBL = ImVec2(uv0.x, uv1.y);

            std::swap(uvTR, uvBL);

            if (flipH)
            {
                std::swap(uvTL, uvTR);
                std::swap(uvBL, uvBR);
            }
            if (flipV)
            {
                std::swap(uvTL, uvBL);
                std::swap(uvTR, uvBR);
            }

            ImVec2 p1(renderPos.x, renderPos.y);
            ImVec2 p2(renderPos.x + renderW, renderPos.y);
            ImVec2 p3(renderPos.x + renderW, renderPos.y + renderH);
            ImVec2 p4(renderPos.x, renderPos.y + renderH);

            bg->AddImageQuad(icon.imTex, p1, p2, p3, p4, uvTL, uvTR, uvBR, uvBL, IM_COL32_WHITE);
        }
        else
        {
            if (flipH)
            {
                std::swap(uv0.x, uv1.x);
            }
            if (flipV)
            {
                std::swap(uv0.y, uv1.y);
            }

            bg->AddImage(icon.imTex, renderPos, ImVec2(renderPos.x + renderW, renderPos.y + renderH),
                         uv0, uv1, IM_COL32_WHITE);
        }
    };

    for (const auto& up : world_->entities())
    {
        if (!up)
            continue;
        if (up.get() == world_->getPlayer())
            continue;
        const Entity& e = *up;
        if (!e.isVisible())
            continue;

        const ImVec2   pos = e.getPosition();
        const uint32_t w   = e.getWidth();
        const uint32_t h   = e.getHeight();
        int iconId;
        float ENTITY_SPRITE_SCALE = 1.0f;
        uint32_t texX = 0;
        uint32_t texY = 0;
        bool flipH = false, flipV = false, flipD = false;

        if (auto* living = dynamic_cast<LivingEntity*>(up.get()))
        {
            ENTITY_SPRITE_SCALE = living->getSpriteScale() * 5.0f;
            if (auto* animationController = living->getAnimationController())
            {
                iconId  = animationController->getCurrentFrameIconId();
                // Animowane encje używają pełnej tekstury (UV 0-1)
            }
            else
            {
                iconId = living->getEntityId();
            }
            drawHpBar(*living, pos.x, pos.y, static_cast<float>(w), living->getSpriteScale());
        }
        else
        {
            if (auto* item = dynamic_cast<Item*>(up.get())) {
                iconId = item->getIconId();
                texX = 0;
                texY = 0;
            } else {
                iconId = e.getEntityId();
                texX = e.getTexX();
                texY = e.getTexY();
                flipH = e.getFlipH();
                flipV = e.getFlipV();
                flipD = e.getFlipD();
            }
        }
        if (iconId >= 0 && assets_) {
            const IconGPU& checkIcon = assets_->icon(iconId);
            if (checkIcon.imTex) {
                drawScaledSprite(iconId, pos, static_cast<float>(w), static_cast<float>(h), ENTITY_SPRITE_SCALE, texX, texY, flipH, flipV, flipD);
            }
        }
    }

    // Rysuj gracza
    if (auto* player = world_->getPlayer())
    {
      const ImVec2   pos = player->getPosition();
        const uint32_t w   = player->getWidth();
        const uint32_t h   = player->getHeight();
        int   iconId;
        if (auto* animationController = player->getAnimationController())
        {
      iconId = animationController->getCurrentFrameIconId();
        }
    else
        {
    iconId = player->getEntityId();
        }

        const float PLAYER_SPRITE_SCALE = player->getSpriteScale() * 5.0f;
        if (iconId >= 0 && assets_) {
            const IconGPU& checkIcon = assets_->icon(iconId);
            if (checkIcon.imTex) {
                drawScaledSprite(iconId, pos, static_cast<float>(w), static_cast<float>(h), PLAYER_SPRITE_SCALE);
            }
        }
        drawHpBar(*player, pos.x, pos.y, static_cast<float>(w), player->getSpriteScale());
 }

    drawInventoryUI();
}

void VulkanImGuiApp::drawInventoryUI()
{
    if (!assets_ || !world_)
        return;

    Player* player = world_->getPlayer();
    if (!player)
        return;

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    Inventory& inv = player->getInventory();
    int  capacity = inv.getInventoryCapacity();

    const float startX = 1920.0f - (capacity * (slotSize + padding));
    const float startY = 8.0f;

    for (int i = 0; i < capacity; ++i)
    {
        float x = startX + i * (slotSize + padding);
        float y = startY;

        ImVec2 slotMin(x, y);
        ImVec2 slotMax(x + slotSize, y + slotSize);
        bg->AddRectFilled(slotMin, slotMax, IM_COL32(40, 40, 40, 200), 4.0f);
        bg->AddRect(slotMin, slotMax, IM_COL32(100, 100, 100, 255), 4.0f);

        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "%d", (i + 1) % 10);
        bg->AddText(ImVec2(x + 2, y + 2), IM_COL32(150, 150, 150, 200), numBuf);

        Item* item = inv.getItem(i);
        if (item && item->getIconId() >= 0 && assets_)
            {
                const auto& icon = assets_->icon(item->getIconId());
                if (icon.imTex) {
                    float iconSize = slotSize - 8.0f;
                    float iconX = x + 4.0f;
                    float iconY = y + 4.0f;
                    bg->AddImage(icon.imTex,
                                ImVec2(iconX, iconY),
                                ImVec2(iconX + iconSize, iconY + iconSize),
                                ImVec2(0, 0), ImVec2(1, 1),
                                IM_COL32_WHITE);
                }
            }
    }

    drawHeartsUI(bg, player);
}

void VulkanImGuiApp::drawHeartsUI(ImDrawList* bg, Player* player)
{
    if (heartIconId_ < 0 || !assets_)
        return;

    const int hp = player->getHp();
    const int maxHp = player->getMaxHp();

    const int hpPerHeart = 10;
    const int totalHearts = (maxHp + hpPerHeart - 1) / hpPerHeart;
    const int fullHearts = hp / hpPerHeart;
    const int partialHp = hp % hpPerHeart;

    const float heartSize = 36.0f;
    const float heartSpacing = 6.0f;
    const float startX = 20.0f;
    const float startY = 10.0f;

    const auto& heartIcon = assets_->icon(heartIconId_);
    if (!heartIcon.imTex)
        return;

    for (int i = 0; i < totalHearts; ++i)
    {
        float x = startX + i * (heartSize + heartSpacing);
        float y = startY;

        ImU32 tintColor;
        if (i < fullHearts)
        {
            tintColor = IM_COL32(255, 255, 255, 255);
        }
        else if (i == fullHearts && partialHp > 0)
        {
            int brightness = 80 + (partialHp * 100 / hpPerHeart);
            tintColor = IM_COL32(brightness, brightness / 3, brightness / 3, 255);
        }
        else
        {
            tintColor = IM_COL32(30, 30, 30, 120);
        }

        bg->AddImage(heartIcon.imTex,
                    ImVec2(x, y),
                    ImVec2(x + heartSize, y + heartSize),
                    ImVec2(0, 0), ImVec2(1, 1),
                    tintColor);
    }
}

void VulkanImGuiApp::drawAttackMode(AttackMode attack_mode) {
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    Player* player = world_->getPlayer();
    if (!player)
        return;
    Inventory& inv = player->getInventory();
    int  capacity = inv.getInventoryCapacity();
    float iconSize = 40.0f;

    float start_x = 1920.0f - (capacity * (slotSize + padding)) - iconSize - 10.0f;
    if (attack_mode == AttackMode::Melee) {

        if (swordIconId_ < 0 || !assets_) {
            std::cout<< "Error loading sword icon"<< std::endl;
            return;
        }

        const IconGPU swordIcon = assets_->icon(swordIconId_);
        if (!swordIcon.imTex) {
            std::cout << "Error: sword icon has no GPU texture" << std::endl;
            return;
        }
        bg->AddImage(swordIcon.imTex,
                     ImVec2(start_x, 8.0f),
                     ImVec2(start_x + iconSize, 8.0f + iconSize),
                     ImVec2(0, 0), ImVec2(1, 1));

    }
    else if ( attack_mode == AttackMode::Ranged) {

        if (bowIconId_ < 0 || !assets_) {
            std::cout<< "Error loading bow icon"<< std::endl;
            return;
        }

        const IconGPU bowIcon = assets_->icon(bowIconId_);
        bg->AddImage(bowIcon.imTex,
                     ImVec2(start_x, 8.0f),
                     ImVec2(start_x + 36.0f, 8.0f + 36.0f),
                     ImVec2(0, 0), ImVec2(1, 1));
    }
}


void VulkanImGuiApp::drawPauseMenu()
{
    auto& io = ImGui::GetIO();

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));

    if (!ImGui::IsPopupOpen("Pauza")) {
        ImGui::OpenPopup("Pauza");
    }

    ImVec2 windowSize(300, 200);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) / 2.0f, (io.DisplaySize.y - windowSize.y) / 2.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;


    if (ImGui::BeginPopupModal("Pauza", nullptr, flags)) {

        // Wycentrowany tekst
        float textWidth = ImGui::CalcTextSize("GRA WSTRZYMANA").x;
        ImGui::SetCursorPosX((windowSize.x - textWidth) / 2.0f);
        ImGui::Text("GRA WSTRZYMANA");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Przyciski wycentrowane
        float buttonWidth = 200.0f;
        float buttonX = (windowSize.x - buttonWidth) / 2.0f;

        ImGui::SetCursorPosX(buttonX);
        if (ImGui::Button("Wznow gre", ImVec2(buttonWidth, 40)))
        {
            isPaused_ = false;
        }

        ImGui::Spacing();

        ImGui::SetCursorPosX(buttonX);
        if (ImGui::Button("Wyjdz z gry", ImVec2(buttonWidth, 40)))
        {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
            ImGui::EndPopup();
    }
    ImGui::PopStyleColor(1);

}

void VulkanImGuiApp::drawDeathView() {
    if (!ImGui::IsPopupOpen("Smierc")) {
        ImGui::OpenPopup("Smierc");
    }
    auto& io = ImGui::GetIO();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));

    ImVec2 windowSize(300, 200);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) / 2.0f, (io.DisplaySize.y - windowSize.y) / 2.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;


    if (ImGui::BeginPopupModal("Smierc", nullptr, flags))
    {

        float textWidth = ImGui::CalcTextSize("Przegrales!").x;
        ImGui::SetCursorPosX((windowSize.x - textWidth) / 2.0f);
        ImGui::Text("Przegrales!");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 200.0f;
        float buttonX = (windowSize.x - buttonWidth) / 2.0f;

        ImGui::SetCursorPosX(buttonX);
        if (ImGui::Button("Odrodzenie", ImVec2(buttonWidth, 40)))
        {
            if (device_)
                vkDeviceWaitIdle(device_);
            resourcesBeingUpdated_ = true;
            restartGame(*world_);
            resourcesBeingUpdated_ = false;

        }

        ImGui::Spacing();

        ImGui::SetCursorPosX(buttonX);
        if (ImGui::Button("Wyjdz z gry", ImVec2(buttonWidth, 40)))
        {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        ImGui::EndPopup();

    }
    ImGui::PopStyleColor(1);
}

void VulkanImGuiApp::drawWinView()
{
    if (!ImGui::IsPopupOpen("Wygrana"))
    {
        ImGui::OpenPopup("Wygrana");
    }
    auto&       io = ImGui::GetIO();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));

    ImVec2 windowSize(300, 200);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) / 2.0f, (io.DisplaySize.y - windowSize.y) / 2.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::BeginPopupModal("Wygrana", nullptr, flags))
    {

        float textWidth = ImGui::CalcTextSize("Wygrales!").x;
        ImGui::SetCursorPosX((windowSize.x - textWidth) / 2.0f);
        ImGui::Text("Wygrales!");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 200.0f;
        float buttonX     = (windowSize.x - buttonWidth) / 2.0f;

        ImGui::SetCursorPosX(buttonX);
        if (ImGui::Button("Odrodzenie", ImVec2(buttonWidth, 40)))
        {
            ImGui::CloseCurrentPopup();
            if (device_)
                vkDeviceWaitIdle(device_);
            resourcesBeingUpdated_ = true;
            restartGame(*world_);
            resourcesBeingUpdated_ = false;
        }

        ImGui::Spacing();

        ImGui::SetCursorPosX(buttonX);
        if (ImGui::Button("Wyjdz z gry", ImVec2(buttonWidth, 40)))
        {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(1);
}

void VulkanImGuiApp::enableAiMode()
{
    aiMode_ = true;
    aiServer_ = std::make_unique<AiServerApp>();
}
