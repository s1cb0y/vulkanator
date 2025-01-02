#include <vknator_engine.h>
#include <vknator_log.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vknator_initializers.h>
#include <vknator_utils.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <vknator_pipelines.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include "glm/gtx/transform.hpp"

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif


bool VknatorEngine::Init(){
    LOG_INFO("Init engine...");

    bool success = true;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        LOG_ERROR("Error SDL2 Initialization : {}", SDL_GetError());
        success = false;
    }
    m_Window = SDL_CreateWindow("VKnator Engine",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    m_WindowExtent.width,
                                    m_WindowExtent.height,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (m_Window == NULL){
        LOG_ERROR("Error window creation");
        success = false;
    }
    LOG_DEBUG("Init vulkan...");        InitVulkan();
    LOG_DEBUG("Init swapchain...");     InitSwapchain();
    LOG_DEBUG("Init commands...");      InitCommands();
    LOG_DEBUG("Init sync structres...");InitSyncStructures();
    LOG_DEBUG("Init descriptors...");   InitDescriptors();
    LOG_DEBUG("Init pipelines...");     InitPipelines();
    LOG_DEBUG("Init default data...");  InitDefaultData();
    LOG_DEBUG("Init ImGui ...");        InitImGui();

    success ? LOG_INFO("Init engine done") : LOG_ERROR("Init engine failed");
    return success;
}


void VknatorEngine::Run(){
    LOG_INFO("Run engine...");
    SDL_Event event;
    while (m_IsRunning){
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_QUIT){
                m_IsRunning = false;
            }
            if (event.type == SDL_WINDOWEVENT){
                if (event.window.event == SDL_WINDOWEVENT_MINIMIZED){
                    m_IsMinimized = true;
                }
                if (event.window.event == SDL_WINDOWEVENT_RESTORED){
                    m_IsMinimized = false;
                }
            }
            ImGui_ImplSDL2_ProcessEvent(&event);
        }
        if (m_IsMinimized){
            //throttle workload if window is minimized
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (m_ResizeRequested){
            ResizeSwapchain();
        }
        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(m_Window);
        ImGui::NewFrame();

        if (ImGui::Begin("background")) {

			ComputeEffect& selected = m_BackgroundEffects[m_CurrentBackgroundEffect];

			ImGui::Text("Selected effect: ", selected.name);

			ImGui::SliderInt("Effect Index", &m_CurrentBackgroundEffect,0, m_BackgroundEffects.size() - 1);

			ImGui::InputFloat4("data1",(float*)& selected.data.data1);
			ImGui::InputFloat4("data2",(float*)& selected.data.data2);
			ImGui::InputFloat4("data3",(float*)& selected.data.data3);
			ImGui::InputFloat4("data4",(float*)& selected.data.data4);

            ImGui::SliderFloat("Render scale", &m_RenderScale, 0.0f, 1.0f);

			ImGui::End();
		}
        //make imgui calculate internal draw structures
        ImGui::Render();

        Draw();
    }
}

void VknatorEngine::Draw(){

    VK_CHECK(vkWaitForFences(m_VkDevice, 1, &GetCurrentFrame().renderFence, true, 1000000000));
    GetCurrentFrame().deletionQueue.Flush();
    GetCurrentFrame().frameDescriptors.clear_pools(m_VkDevice);
    VK_CHECK(vkResetFences(m_VkDevice, 1, &GetCurrentFrame().renderFence));

    uint32_t swapChainImageIndex;
    VkResult result = vkAcquireNextImageKHR(m_VkDevice, m_SwapChain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapChainImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR){
        m_ResizeRequested = true;
        return;
    }
    VkCommandBuffer cmd = GetCurrentFrame().mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = vknatorinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

//> draw_first
	m_DrawExtent.width  = std::min(m_DrawImage.imageExtent.width, m_SwapChainExtent.width) * m_RenderScale;
	m_DrawExtent.height = std::min(m_DrawImage.imageExtent.height, m_SwapChainExtent.height) * m_RenderScale;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    // transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
    vknatorutils::TransitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    //make a clear-color from frame number. This will flash with a 120 frame period.
    DrawBackground(cmd);

    vknatorutils::TransitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vknatorutils::TransitionImage(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    DrawGeometry(cmd);

    //make the swapchain image into presentable mode
    vknatorutils::TransitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vknatorutils::TransitionImage(cmd, m_SwapChainImages[swapChainImageIndex],VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
//< draw_first
//> imgui_draw
	// execute a copy from the draw image into the swapchain
	vknatorutils::CopyImageToImage(cmd, m_DrawImage.image, m_SwapChainImages[swapChainImageIndex], m_DrawExtent, m_SwapChainExtent);

	// set swapchain image layout to Attachment Optimal so we can draw it
	vknatorutils::TransitionImage(cmd, m_SwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//draw imgui into the swapchain image
	DrawImgui(cmd,  m_SwapChainImageViews[swapChainImageIndex]);

	// set swapchain image layout to Present so we can draw it
	vknatorutils::TransitionImage(cmd, m_SwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));
//< imgui_draw

    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vknatorinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vknatorinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,GetCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vknatorinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);

    VkSubmitInfo2 submit = vknatorinit::submit_info(&cmdinfo,&signalInfo,&waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, GetCurrentFrame().renderFence));
    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &m_SwapChain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapChainImageIndex;

    result = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR){
        m_ResizeRequested = true;
    }
    m_FrameNumber++;
}

void VknatorEngine::DrawBackground(VkCommandBuffer cmd)
{
    //get current effect
    ComputeEffect& effect = m_BackgroundEffects[m_CurrentBackgroundEffect];
	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipelineLayout, 0, 1, &m_DrawImageDescriptors, 0, nullptr);

    ComputePushConstants pc;
    pc.data1 = glm::vec4{1,0,0,1};
    pc.data2 = glm::vec4{0,0,1,1};

    vkCmdPushConstants(cmd, m_GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
}

void VknatorEngine::DrawGeometry(VkCommandBuffer cmd){
    //begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vknatorinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vknatorinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vknatorinit::rendering_info(m_DrawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshPipeline);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = m_DrawExtent.width;
	viewport.height = m_DrawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

    // bind a texture
    VkDescriptorSet imageSet = GetCurrentFrame().frameDescriptors.allocate(m_VkDevice, m_SingeImageDescriptorLayout);
    {
        DescriptorWriter writer;
        writer.write_image(0, m_ErrorCheckerboardImage.imageView, m_DefaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.update_set(m_VkDevice, imageSet);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshPipelineLayout, 0, 1, &imageSet, 0, nullptr);
    // draw monkey head
    glm::mat4 view = glm::translate(glm::vec3{0, 0, -5});
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)m_DrawExtent.width / (float)m_DrawExtent.height, 0.1f, 10000.f);

    // invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
    projection[1][1] *= -1;
	GPUDrawPushConstants push_constants;
    push_constants.worldMatrix =  projection * view;
    push_constants.vertexBuffer = m_testMeshes[2]->meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(cmd, m_MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(cmd, m_testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, m_testMeshes[2]->surfaces[0].count, 1, m_testMeshes[2]->surfaces[0].startIndex, 0, 0);

	vkCmdEndRendering(cmd);

    AllocatedBuffer gpuSceneBuffer = CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    GetCurrentFrame().deletionQueue.PushFunction([=, this](){
        DestroyBuffer(gpuSceneBuffer);
    });

    //write data to buffer
    GPUSceneData* gpuSceneData = (GPUSceneData*)gpuSceneBuffer.allocation->GetMappedData();
    *gpuSceneData = m_SceneData;

    //create descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = GetCurrentFrame().frameDescriptors.allocate(m_VkDevice, m_GPUSceneDataDescriptorSetLayout);
    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(m_VkDevice, globalDescriptor);


}

void VknatorEngine::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView){
    VkRenderingAttachmentInfo colorAttachment = vknatorinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
	VkRenderingInfo renderInfo = vknatorinit::rendering_info(m_SwapChainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void VknatorEngine::Deinit(){
    LOG_INFO("Shut down engine...");

    //wait for GPU to stop
    vkDeviceWaitIdle(m_VkDevice);

    for (auto& mesh : m_testMeshes){
        DestroyBuffer(mesh->meshBuffers.indexBuffer);
        DestroyBuffer(mesh->meshBuffers.vertexBuffer);
    }

    // destroy command pools, which destroy all allocated command buffers
    for (int i = 0; i < FRAME_OVERLAP; i++){
        vkDestroyCommandPool(m_VkDevice, m_Frames[i].commandPool, nullptr);
        //destroy sync objects
        vkDestroyFence(m_VkDevice, m_Frames[i].renderFence, nullptr);
        vkDestroySemaphore(m_VkDevice, m_Frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(m_VkDevice ,m_Frames[i].swapchainSemaphore, nullptr);
        m_Frames[i].deletionQueue.Flush();
    }

    m_MainDeletionQueue.Flush();

    // destroy swapchain resources
    DestroySwapchain();

    vkDestroySurfaceKHR(m_VkInstance, m_VkSurface, nullptr);

    vkDestroyDevice(m_VkDevice, nullptr);
    vkb::destroy_debug_utils_messenger(m_VkInstance, m_VkDebugMessenger);
    vkDestroyInstance(m_VkInstance, nullptr);
    //Destroy window
    SDL_DestroyWindow( m_Window );
    m_Window = NULL;

    //Quit SDL subsystems
    SDL_Quit();
    LOG_DEBUG("Done");
}

void VknatorEngine::InitVulkan(){
    vkb::InstanceBuilder instanceBuilder;
    //create the vulkans instance
    auto instance = instanceBuilder.set_app_name("Vulkanator Engine")
    .request_validation_layers(enableValidationLayers)
    .use_default_debug_messenger()
    .require_api_version(1, 3, 0)
    .build();
    vkb::Instance vkbInstance = instance.value();
    // grab VKInstance from vkb istance
    m_VkInstance = vkbInstance.instance;
    // get default debug messenger
    m_VkDebugMessenger = vkbInstance.debug_messenger;

    //We want a gpu that can write to the SDL surface and supports vulkan 1.3
    SDL_Vulkan_CreateSurface(m_Window, m_VkInstance, &m_VkSurface);

    //use vkbootstrap to select a gpu.
    VkPhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{ vkbInstance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(m_VkSurface)
        .select()
        .value();
    LOG_INFO("GPU used: {}",  physicalDevice.name);

    vkb::DeviceBuilder deviceBuilder {physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();
    m_VkDevice = vkbDevice.device;
    m_ActiveGPU = physicalDevice.physical_device;

    m_GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    m_GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    // Init memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_ActiveGPU;
    allocatorInfo.device = m_VkDevice;
    allocatorInfo.instance = m_VkInstance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_Allocator);
    m_MainDeletionQueue.PushFunction([&](){vmaDestroyAllocator(m_Allocator);}) ;

}

void VknatorEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{ m_ActiveGPU,m_VkDevice, m_VkSurface };

    m_SwapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = m_SwapChainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        //use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    m_SwapChainExtent = vkbSwapchain.extent;
    //store swapchain and its related images
    m_SwapChain = vkbSwapchain.swapchain;
    m_SwapChainImages = vkbSwapchain.get_images().value();
    m_SwapChainImageViews = vkbSwapchain.get_image_views().value();
}

void VknatorEngine::InitSwapchain()
{
    CreateSwapchain(m_WindowExtent.width, m_WindowExtent.height);

//> init_swap
    //draw image size will match the window
    VkExtent3D drawImageExtent = {
        m_WindowExtent.width,
        m_WindowExtent.height,
        1
    };
//> Draw image

    m_DrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_DrawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vknatorinit::image_create_info(m_DrawImage.imageFormat, drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the images
    vmaCreateImage(m_Allocator, &rimg_info, &rimg_allocinfo, &m_DrawImage.image, &m_DrawImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vknatorinit::imageview_create_info(m_DrawImage.imageFormat, m_DrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(m_VkDevice, &rview_info, nullptr, &m_DrawImage.imageView));

//> Depth image

    //hardcoding the draw format to 32 bit float
    m_DepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_DepthImage.imageExtent = drawImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vknatorinit::image_create_info(m_DepthImage.imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(m_Allocator, &dimg_info, &rimg_allocinfo, &m_DepthImage.image, &m_DepthImage.allocation, nullptr);

    VkImageViewCreateInfo dview_info = vknatorinit::imageview_create_info(m_DepthImage.imageFormat, m_DepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(m_VkDevice, &dview_info, nullptr, &m_DepthImage.imageView));

    //add to deletion queues
    m_MainDeletionQueue.PushFunction([=, *this]() {
        vkDestroyImageView(m_VkDevice, m_DrawImage.imageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);

        vkDestroyImageView(m_VkDevice, m_DepthImage.imageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DepthImage.image, m_DepthImage.allocation);
    });
//< init_swap

}

void VknatorEngine::DestroySwapchain(){
    vkDestroySwapchainKHR(m_VkDevice, m_SwapChain, nullptr);
    for (int i = 0; i < m_SwapChainImageViews.size(); i++) {
        vkDestroyImageView(m_VkDevice, m_SwapChainImageViews[i], nullptr);
    }
}

void VknatorEngine::ResizeSwapchain(){
    vkDeviceWaitIdle(m_VkDevice);

    DestroySwapchain();
    int h, w;
    SDL_GetWindowSize(m_Window, &w, &h);
    m_WindowExtent.height = h;
    m_WindowExtent.width = w;

    CreateSwapchain(m_WindowExtent.width, m_WindowExtent.height);
    m_ResizeRequested = false;
}

void VknatorEngine::InitCommands(){
    VkCommandPoolCreateInfo cmdPoolInfo = vknatorinit::command_pool_create_info(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++){
        VK_CHECK(vkCreateCommandPool(m_VkDevice, &cmdPoolInfo, nullptr, &m_Frames[i].commandPool));
        VkCommandBufferAllocateInfo cmdBufferAllocInfo = vknatorinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(m_VkDevice, &cmdBufferAllocInfo, &m_Frames[i].mainCommandBuffer));
    }
//> imm_cmd
    VK_CHECK(vkCreateCommandPool(m_VkDevice, &cmdPoolInfo, nullptr, &m_ImmCommandPool));
    VkCommandBufferAllocateInfo cmdBufferAllocInfo = vknatorinit::command_buffer_allocate_info(m_ImmCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(m_VkDevice, &cmdBufferAllocInfo, &m_ImmCommandBuffer));
    m_MainDeletionQueue.PushFunction([=, *this](){vkDestroyCommandPool(m_VkDevice, m_ImmCommandPool, nullptr);});
//< imm_cmd
}

void VknatorEngine::InitSyncStructures(){
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to syncronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vknatorinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vknatorinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(m_VkDevice, &fenceCreateInfo, nullptr, &m_Frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(m_VkDevice, &semaphoreCreateInfo, nullptr, &m_Frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(m_VkDevice, &semaphoreCreateInfo, nullptr, &m_Frames[i].renderSemaphore));
    }
 //> imm_sync
    VK_CHECK(vkCreateFence(m_VkDevice, &fenceCreateInfo, nullptr, &m_ImmFence));
    m_MainDeletionQueue.PushFunction([=, *this](){vkDestroyFence(m_VkDevice, m_ImmFence, nullptr);});
 //< imm_sync
}

void VknatorEngine::InitDescriptors(){
    //create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};

    m_GlobalDescriptorAllocator.init(m_VkDevice, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		m_DrawImageDescriptorLayout = builder.build(m_VkDevice, VK_SHADER_STAGE_COMPUTE_BIT);
	}
    // make the descriptor set for the scene data
    {
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		m_GPUSceneDataDescriptorSetLayout = builder.build(m_VkDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

    // make the descriptor set for the checkerboard texutre data
    {
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		m_SingeImageDescriptorLayout = builder.build(m_VkDevice, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	//allocate a descriptor set for our draw image
	m_DrawImageDescriptors = m_GlobalDescriptorAllocator.allocate(m_VkDevice, m_DrawImageDescriptorLayout);

	DescriptorWriter writer;
    writer.write_image(0, m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(m_VkDevice, m_DrawImageDescriptors);

    for (int i = 0; i < FRAME_OVERLAP; i++){
    // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };
        m_Frames[i].frameDescriptors = DescriptorAllocatorGrowable();
        m_Frames[i].frameDescriptors.init(m_VkDevice, 1000, frameSizes);

        m_MainDeletionQueue.PushFunction([&,i](){
            m_Frames[i].frameDescriptors.destroy_pools(m_VkDevice);
        });
    }
    m_MainDeletionQueue.PushFunction([&](){
        m_GlobalDescriptorAllocator.destroy_pools(m_VkDevice);
        vkDestroyDescriptorSetLayout(m_VkDevice, m_DrawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_VkDevice, m_GPUSceneDataDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_VkDevice, m_SingeImageDescriptorLayout, nullptr);
    });
}

void VknatorEngine::InitPipelines(){
    // COMPUTE PIPELINE
    InitBackgroundPipelines();
    // GRAPHICS PIPELINE
    InitMeshPipeline();
}

void VknatorEngine::InitBackgroundPipelines(){

    VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(m_VkDevice, &computeLayout, nullptr, &m_GradientPipelineLayout));

    //layout code
	VkShaderModule gradientShader;
	if (!vknatorutils::LoadShaderModule("../shaders/gradient_color.comp.spv", m_VkDevice, &gradientShader))
	{
		LOG_ERROR("Error when building the gradient shader");
	}
    VkShaderModule skyShader;
	if (!vknatorutils::LoadShaderModule("../shaders/sky.comp.spv", m_VkDevice, &skyShader))
	{
		LOG_ERROR("Error when building the sky shader");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = m_GradientPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

    ComputeEffect gradient;
    gradient.layout = m_GradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    //default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(m_VkDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

    //change the shader module only to create the sky shader
    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = m_GradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};

    //default sky parameters
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4 ,0.97);
    VK_CHECK(vkCreateComputePipelines(m_VkDevice, VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &sky.pipeline));

    // add the 2 background effects into array
    m_BackgroundEffects.push_back(gradient);
    m_BackgroundEffects.push_back(sky);
    //destroy shader modules properly
    vkDestroyShaderModule(m_VkDevice, gradientShader, nullptr);
    vkDestroyShaderModule(m_VkDevice, skyShader, nullptr);

	m_MainDeletionQueue.PushFunction([&]() {
		vkDestroyPipelineLayout(m_VkDevice, m_GradientPipelineLayout, nullptr);
        vkDestroyPipeline(m_VkDevice, m_BackgroundEffects[0].pipeline, nullptr);
        vkDestroyPipeline(m_VkDevice, m_BackgroundEffects[1].pipeline, nullptr);
    });
}

void VknatorEngine::InitMeshPipeline(){

	VkShaderModule triangleFragShader;
	if (!vknatorutils::LoadShaderModule("../shaders/text_image.frag.spv", m_VkDevice, &triangleFragShader))
	{
		LOG_ERROR("Error when building the triangle fragment shader");
	}
    VkShaderModule triangleVertexShader;
	if (!vknatorutils::LoadShaderModule("../shaders/colored_triangle_mesh.vert.spv", m_VkDevice, &triangleVertexShader))
	{
		LOG_ERROR("Error when building the triangle mesh vertex shader");
	}

    //build the pipeline layout that controls the inputs/outputs of the shader
	//we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vknatorinit::pipeline_layout_create_info();
    pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_SingeImageDescriptorLayout;
    pipelineLayoutInfo.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_VkDevice, &pipelineLayoutInfo, nullptr, &m_MeshPipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.m_PipelineLayout = m_MeshPipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.SetShaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.SetMultisamplingNone();
	//no blending
	pipelineBuilder.DisableBlending();
	// depth testing
    pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	//connect the image format we will draw into, from draw image
	pipelineBuilder.SetColorAttachmentFormat(m_DrawImage.imageFormat);
	pipelineBuilder.SetDepthFormat(m_DepthImage.imageFormat);
    // enable blending
    pipelineBuilder.EnableBlendingAdditive();
	//finally build the pipeline
	m_MeshPipeline = pipelineBuilder.BuildPipeline(m_VkDevice);

	//clean structures
	vkDestroyShaderModule(m_VkDevice, triangleFragShader, nullptr);
	vkDestroyShaderModule(m_VkDevice, triangleVertexShader, nullptr);

	m_MainDeletionQueue.PushFunction([&]() {
		vkDestroyPipelineLayout(m_VkDevice, m_MeshPipelineLayout, nullptr);
		vkDestroyPipeline(m_VkDevice, m_MeshPipeline, nullptr);
	});

}
void VknatorEngine::InitImGui(){
    /// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(m_VkDevice, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(m_Window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_VkInstance;
	init_info.PhysicalDevice = m_ActiveGPU;
	init_info.Device = m_VkDevice;
	init_info.Queue = m_GraphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;
	init_info.ColorAttachmentFormat = m_SwapChainImageFormat;

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

	// execute a gpu command to upload imgui font textures
	ImmediateSubmit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

	// clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	// add the destroy the imgui created structures
	m_MainDeletionQueue.PushFunction([=, *this]() {
		vkDestroyDescriptorPool(m_VkDevice, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
	});
}

void VknatorEngine::InitDefaultData() {

    m_testMeshes = loadGltfMeshes(this, "../assets/basicmesh.glb").value();

    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1 ,1));
    m_WhiteImage = CreateImage((void*) &white, VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f ,1));
    m_GreyImage = CreateImage((void*) &grey, VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0 ,0));
    m_BlackImage = CreateImage((void*) &black, VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    std::array<uint32_t, 16*16> pixels;
    uint32_t mangenta = glm::packUnorm4x8(glm::vec4(1, 0, 1 ,1));
    for (int x = 0; x < 16; x++){
        for (int y = 0; y < 16; y++){
            pixels[x + y * 16] = ((x % 2) ^ (y % 2)) ? mangenta : black;
        }
    }
    m_ErrorCheckerboardImage = CreateImage((void*) pixels.data(), VkExtent3D{16,16,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST
    };
    vkCreateSampler(m_VkDevice, &sampl_info, nullptr, &m_DefaultSamplerNearest);

    sampl_info.magFilter = VK_FILTER_LINEAR;
    sampl_info.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(m_VkDevice, &sampl_info, nullptr, &m_DefaultSamplerLinear);

    m_MainDeletionQueue.PushFunction([&](){
        vkDestroySampler(m_VkDevice, m_DefaultSamplerNearest, nullptr);
        vkDestroySampler(m_VkDevice, m_DefaultSamplerLinear, nullptr);
        DestroyImage(m_WhiteImage);
        DestroyImage(m_GreyImage);
        DestroyImage(m_BlackImage);
        DestroyImage(m_ErrorCheckerboardImage);
    });

}

void VknatorEngine::ImmediateSubmit(std::function<void(VkCommandBuffer &cmd)>&&function){
    VK_CHECK(vkResetFences(m_VkDevice, 1, &m_ImmFence));
	VK_CHECK(vkResetCommandBuffer(m_ImmCommandBuffer, 0));

	VkCommandBuffer cmd = m_ImmCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = vknatorinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = vknatorinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vknatorinit::submit_info(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, m_ImmFence));

	VK_CHECK(vkWaitForFences(m_VkDevice, 1, &m_ImmFence, true, 9999999999));
}

AllocatedBuffer VknatorEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage){
    //allocate the buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

void VknatorEngine::DestroyBuffer(const AllocatedBuffer& buffer){
    vmaDestroyBuffer(m_Allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VknatorEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	//create vertex buffer
	newSurface.vertexBuffer = CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							VMA_MEMORY_USAGE_GPU_ONLY);

	//find the adress of the vertex buffer
	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_VkDevice, &deviceAdressInfo);

	//create index buffer
	newSurface.indexBuffer = CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // create temporal CPU writable staging buffer
    AllocatedBuffer staging = CreateBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.allocation->GetMappedData();

	// copy vertex buffer
	memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	ImmediateSubmit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
	});

	DestroyBuffer(staging);

	return newSurface;
}

AllocatedImage VknatorEngine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped){
    AllocatedImage image;
    image.imageExtent = size;
    image.imageFormat= format;

    VkImageCreateInfo imgCreateInfo = vknatorinit::image_create_info(image.imageFormat, usage, image.imageExtent);
    if (mipmapped){
        imgCreateInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }
    //always allocate images on dedicated GPU memeory
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //allocate and create image
    VK_CHECK(vmaCreateImage(m_Allocator, &imgCreateInfo, &vmaAllocInfo, &image.image, &image.allocation, nullptr));

    // if the format is a depth image, the correct aspect flag must be used
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (VK_FORMAT_D32_SFLOAT == format){
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo imgViewCreateInfo = vknatorinit::imageview_create_info(format, image.image, aspectFlags);
    imgViewCreateInfo.subresourceRange.levelCount = imgCreateInfo.mipLevels;
    VK_CHECK(vkCreateImageView(m_VkDevice, &imgViewCreateInfo, nullptr, &image.imageView));

    return image;
}

AllocatedImage VknatorEngine::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped){

    size_t dataSize = size.depth * size.height * size.width * 4;
    AllocatedBuffer uploadBuffer = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    memcpy(uploadBuffer.info.pMappedData, data, dataSize);

    AllocatedImage new_image = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	ImmediateSubmit([&](VkCommandBuffer cmd) {
		vknatorutils::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion);

		vknatorutils::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});


    DestroyBuffer(uploadBuffer);
    return new_image;
}

void VknatorEngine::DestroyImage(const AllocatedImage& img){
    vkDestroyImageView(m_VkDevice, img.imageView, nullptr);
    vmaDestroyImage(m_Allocator, img.image, img.allocation);
}


