#include <vknator_pipelines.h>
#include <fstream>
#include <vknator_log.h>
//> load_shader
bool vknatorutils::LoadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule){
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()){
        LOG_ERROR("Could not open file {}", filePath);
        return false;
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    //load the entire file
    file.read((char*)buffer.data(), fileSize);

    VkShaderModuleCreateInfo shaderCreateInfo{};
    shaderCreateInfo.pNext = nullptr;
    shaderCreateInfo.codeSize = buffer.size() * sizeof(uint32_t);
    shaderCreateInfo.pCode = buffer.data();
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    VkShaderModule shaderModule;
    // Check if shader module could be loaded successfully
    if (vkCreateShaderModule(device, &shaderCreateInfo, nullptr, &shaderModule) != VK_SUCCESS){
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}