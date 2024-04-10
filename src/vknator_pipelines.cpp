#include <vknator_pipelines.h>
#include <fstream>
#include <vknator_log.h>
#include <vknator_initializers.h>

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
//< load_shader

//> pipeline builder
void PipelineBuilder::Clear(){
    //clear all of the structs we need back to 0 with their correct stype

	m_InputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

	m_Rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

	m_ColorBlendAttachment = {};

	m_Multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

	m_PipelineLayout = {};

	m_DepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

	m_RenderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

	m_ShaderStages.clear();
}

VkPipeline PipelineBuilder::BuildPipeline(VkDevice device){
    //make viewport state form our stored viewport and scissor
    //at the moment wi wont support multiple viewports and scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount =1;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &m_ColorBlendAttachment;


    //completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    //connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &m_RenderInfo;

    pipelineInfo.stageCount = (uint32_t)m_ShaderStages.size();
    pipelineInfo.pStages = m_ShaderStages.data();
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_InputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &m_Rasterizer;
    pipelineInfo.pMultisampleState = &m_Multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &m_DepthStencil;
    pipelineInfo.layout = m_PipelineLayout;

    VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
            nullptr, &newPipeline)
        != VK_SUCCESS) {
        LOG_ERROR("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    } else {
        return newPipeline;
    }
}

void PipelineBuilder::SetShaders(VkShaderModule vertexShader, VkShaderModule fragementShader){
    m_ShaderStages.clear();
    m_ShaderStages.push_back(vknatorinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
    m_ShaderStages.push_back(vknatorinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragementShader));
}

void PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology){
    m_InputAssembly.topology = topology;
    // we are not going to use primitive restart on the entire tutorial so leave
	// it on false
    m_InputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::SetPolygonMode(VkPolygonMode mode){
    m_Rasterizer.polygonMode = mode;
    m_Rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace){
    m_Rasterizer.cullMode = cullMode;
    m_Rasterizer.frontFace = frontFace;
}

void PipelineBuilder::SetMultisamplingNone()
{
    m_Multisampling.sampleShadingEnable = VK_FALSE;
	// multisampling defaulted to no multisampling (1 sample per pixel)
    m_Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_Multisampling.minSampleShading = 1.0f;
	m_Multisampling.pSampleMask = nullptr;
    //no alpha to coverage either
	m_Multisampling.alphaToCoverageEnable = VK_FALSE;
    m_Multisampling.alphaToOneEnable = VK_FALSE;
}
void PipelineBuilder::DisableBlending()
{
    //default write mask
    m_ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    //no blending
    m_ColorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::SetColorAttachmentFormat(VkFormat format)
{
	m_ColorAttachmentformat = format;
	//connect the format to the renderInfo  structure
	m_RenderInfo.colorAttachmentCount = 1;
	m_RenderInfo.pColorAttachmentFormats = &m_ColorAttachmentformat;
}

void PipelineBuilder::SetDepthFormat(VkFormat format)
{
    m_RenderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::DisableDepthtest()
{
	m_DepthStencil.depthTestEnable = VK_FALSE;
	m_DepthStencil.depthWriteEnable = VK_FALSE;
	m_DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
	m_DepthStencil.stencilTestEnable = VK_FALSE;
	m_DepthStencil.front = {};
	m_DepthStencil.back = {};
	m_DepthStencil.minDepthBounds = 0.f;
	m_DepthStencil.maxDepthBounds= 1.f;
}


//< pipeline builder
