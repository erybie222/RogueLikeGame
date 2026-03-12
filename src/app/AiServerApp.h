#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>

class World;

class AiServerApp
{
public:
    ~AiServerApp();

    void start();
    void stop();

    int consumeDirection();
    void updateResponse(int hp, bool alive);

private:
    void serverLoop();

    std::mutex responseMutex_;
    nlohmann::json response;
    std::thread serverThread_;
    std::atomic<bool> running_{false};
    std::atomic<int> lastDirection_{-1};
};