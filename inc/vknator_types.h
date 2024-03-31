#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include "vknator_log.h"
#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

// we will add our main reusable types here
struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

#define VK_CHECK(x)                                                   \
    if (x){                                                           \
        VkResult vkRes = x;                                           \
        LOG_ERROR("Vulkan error occured: {}", string_VkResult(vkRes));\
        abort();                                                      \
    }
