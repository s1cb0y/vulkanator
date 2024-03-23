#pragma once

#include <vulkan/vulkan.h>

namespace vknatorutils{
    void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
}