#pragma once

#include <vknator_types.h>

namespace vknatorutils {
    bool LoadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}