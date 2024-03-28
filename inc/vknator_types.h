#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include "vknator_log.h"
#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

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