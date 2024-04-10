#pragma once

#include <vknator_types.h>

namespace vknatorutils {
    bool LoadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}

//< pipeline builder
class PipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;

    VkPipelineInputAssemblyStateCreateInfo m_InputAssembly;
    VkPipelineRasterizationStateCreateInfo m_Rasterizer;
    VkPipelineColorBlendAttachmentState m_ColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo m_Multisampling;
    VkPipelineLayout m_PipelineLayout;
    VkPipelineDepthStencilStateCreateInfo m_DepthStencil;
    VkPipelineRenderingCreateInfo m_RenderInfo;
    VkFormat m_ColorAttachmentformat;

    PipelineBuilder(){ Clear(); }

    void Clear();
    void SetShaders(VkShaderModule vertexShader, VkShaderModule fragementShader);
    void SetInputTopology(VkPrimitiveTopology topology);
    void SetPolygonMode(VkPolygonMode mode);
    void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void SetMultisamplingNone();
    void DisableBlending();
    void SetColorAttachmentFormat(VkFormat format);
    void SetDepthFormat(VkFormat format);
    void DisableDepthtest();

    VkPipeline BuildPipeline(VkDevice device);
};
//> pipeline builder