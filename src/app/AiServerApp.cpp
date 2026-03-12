#include "app/AiServerApp.h"

#include <iostream>
#include <string>
#include <zmq.hpp>
#include <nlohmann/json.hpp>


AiServerApp::~AiServerApp()
{
    stop();
}

void AiServerApp::start()
{
    if (running_)
        return;
    running_ = true;
    serverThread_ = std::thread(&AiServerApp::serverLoop, this);
}

void AiServerApp::stop()
{
    running_ = false;
    if (serverThread_.joinable())
        serverThread_.join();
}

int AiServerApp::consumeDirection()
{
    return lastDirection_.exchange(-1);
}

void AiServerApp::updateResponse(int hp, bool alive, const std::vector<std::vector<int>>& grid) {
    std::lock_guard<std::mutex> lock(responseMutex_);
    response["hp"] = hp;
    response["alive"] = alive;
    response["grid"] = grid;
}

void AiServerApp::serverLoop()
{
    try
    {
        zmq::context_t context(1);
        zmq::socket_t  socket(context, zmq::socket_type::rep);

        socket.set(zmq::sockopt::rcvtimeo, 100);
        socket.bind("tcp://*:5555");

        std::cout << "[AI] Serwer uruchomiony na tcp://*:5555" << std::endl;

        while (running_)
        {
            zmq::message_t request;
            if (!socket.recv(request, zmq::recv_flags::none))
                continue;

            const std::string command(static_cast<const char*>(request.data()), request.size());

            if (command == "quit")
            {
                socket.send(zmq::buffer(response.dump()), zmq::send_flags::none);
                running_ = false;
                break;
            }

            if (command.size() == 1 && command[0] >= '0' && command[0] <= '3')
            {
                lastDirection_.store(command[0] - '0');
                std::lock_guard<std::mutex> lock(responseMutex_);
                socket.send(zmq::buffer(response.dump()), zmq::send_flags::none);
            }
            else
            {
                socket.send(zmq::buffer(std::string("unknown_command")), zmq::send_flags::none);
            }
        }

        std::cout << "[AI] Serwer zamkniety." << std::endl;
    }
    catch (const zmq::error_t& e)
    {
        if (running_)
            std::cerr << "[AI] Blad ZMQ: " << e.what() << std::endl;
    }
}