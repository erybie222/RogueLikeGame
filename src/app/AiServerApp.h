#pragma once

#include <atomic>
#include <thread>

class AiServerApp
{
public:
    ~AiServerApp();

    void start();
    void stop();

    int consumeDirection();

private:
    void serverLoop();

    std::thread serverThread_;
    std::atomic<bool> running_{false};
    std::atomic<int> lastDirection_{-1};
};