#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>

class World;

class AiServerApp
{
public:
    ~AiServerApp();

    void start();
    void stop();

    int consumeDirection();
    void updateResponse(int hp, bool alive, int playerTileX, int playerTileY, const std::vector<std::vector<int>>& grid);

private:
    void serverLoop();

    std::mutex responseMutex_;
    nlohmann::json response;
    std::thread serverThread_;
    std::atomic<bool> running_{false};
    std::atomic<int> lastDirection_{-1};
};