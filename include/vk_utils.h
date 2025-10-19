#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>

namespace vkutils {
inline void checkVk(VkResult res, const char* msg) {
    if (res != VK_SUCCESS) {
        std::cerr << msg << " VkResult=" << res << std::endl;
        throw std::runtime_error(msg);
    }
}
} // namespace vkutils

