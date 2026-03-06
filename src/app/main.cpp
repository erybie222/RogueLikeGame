#include "engine/gfx/VulkanImGuiApp.h"

#include <iostream>
#include <string>
#include <zmq.hpp>

namespace
{
    int runAiServer() {
        try
        {
            zmq::context_t context(1);
            zmq::socket_t  socket(context, zmq::socket_type::rep);

            socket.bind("tcp://localhost:5555");

            std::cout << "[AI] Serwer uruchomiony na tcp://localhost:5555" << std::endl;
            std::cout << "[AI] Czekam na wiadomosci..." << std::endl;

            while (true)
            {
                zmq::message_t request;
                const zmq::recv_result_t result = socket.recv(request, zmq::recv_flags::none);

                if (!result)
                {
                    continue;
                }

                const std::string command(static_cast<const char*>(request.data()), request.size());
                std::cout << "[AI] Odebrano: " << command << std::endl;

                std::string response;
                if (command == "quit")
                {
                    response = "ok";
                    socket.send(zmq::buffer(response), zmq::send_flags::none);
                    std::cout << "[AI] Zamkniecie serwera." << std::endl;
                    break;
                }

                response = "C++ odebral: " + command;
                socket.send(zmq::buffer(response), zmq::send_flags::none);
            }

            return 0;
        }
        catch (const zmq::error_t& e)
        {
            std::cerr << "[AI] Blad ZMQ: " << e.what() << std::endl;
            return 1;
        }
    }
}

int main(int argc, char** argv)
{
    bool aiMode = false;

    for (int i = 1; i < argc; ++i)
    {
        if (!argv[i])
        {
            continue;
        }

        const std::string arg = argv[i];

        if (arg == "--smoke")
        {
            VulkanImGuiApp app;
            return app.runSmokeTest();
        }

        if (arg == "--ai-mode")
        {
            aiMode = true;
        }
    }

    if (aiMode)
    {
        return runAiServer();
    }

    VulkanImGuiApp app;
    return app.run();
}
