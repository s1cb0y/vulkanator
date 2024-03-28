#include "vknator_engine.h"
#include "vknator_log.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <VkBootstrap.h>
#include "vknator_initializers.h"
#include <vknator_utils.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif


bool VknatorEngine::Init(){
    LOG_DEBUG("Init engine...");

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
                                    SDL_WINDOW_VULKAN);
    if (m_Window == NULL){
        LOG_ERROR("Error window creation");
        success = false;
    }

    InitVulkan();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();

    success ? LOG_DEBUG("Init engine done") : LOG_DEBUG("Init engine failed");
    return success;
}

void VknatorEngine::Run(){
    LOG_DEBUG("Run engine...");
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
        }
        if (m_IsMinimized){
            //throttle workload if window is minimized
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        Draw();
    }
}

void VknatorEngine::Draw(){

    VK_CHECK(vkWaitForFences(m_VkDevice, 1, &GetCurrentFrame().renderFence, true, 1000000000));
    GetCurrentFrame().deletionQueue.Flush();
    VK_CHECK(vkResetFences(m_VkDevice, 1, &GetCurrentFrame().renderFence));

    uint32_t swapChainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(m_VkDevice, m_SwapChain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapChainImageIndex));
    VkCommandBuffer cmd = GetCurrentFrame().mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = vknatorinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

//> draw_first
	m_DrawExtent.width = m_DrawImage.imageExtent.width;
	m_DrawExtent.height = m_DrawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    // transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
    vknatorutils::TransitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    //make a clear-color from frame number. This will flash with a 120 frame period.
    DrawBackground(cmd);

    //make the swapchain image into presentable mode
    vknatorutils::TransitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vknatorutils::TransitionImage(cmd, m_SwapChainImages[swapChainImageIndex],VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // execute a copy from the draw image into the swapchain
    vknatorutils::CopyImageToImage(cmd, m_DrawImage.image, m_SwapChainImages[swapChainImageIndex], m_DrawExtent, m_SwapChainExtent);
    // set swapchain image layout to Present so we can show it on the screen
    vknatorutils::TransitionImage(cmd, m_SwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
//< draw_first

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

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

    VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

    m_FrameNumber++;
}

void VknatorEngine::DrawBackground(VkCommandBuffer cmd)
{
	//make a clear-color from frame number. This will flash with a 120 frame period.
	VkClearColorValue clearValue;
	float flash = abs(sin(m_FrameNumber / 120.f));
	clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

	VkImageSubresourceRange clearRange = vknatorinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	vkCmdClearColorImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}

void VknatorEngine::Deinit(){
    LOG_DEBUG("Shut down engine...");

    //wait for GPU to stop
    vkDeviceWaitIdle(m_VkDevice);
    m_MainDeletionQueue.Flush();
    // destroy command pools, which destroy all allocated command buffers
    for (int i = 0; i < FRAME_OVERLAP; i++){
        vkDestroyCommandPool(m_VkDevice, m_Frames[i].commandPool, nullptr);
        //destroy sync objects
        vkDestroyFence(m_VkDevice, m_Frames[i].renderFence, nullptr);
        vkDestroySemaphore(m_VkDevice, m_Frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(m_VkDevice ,m_Frames[i].swapchainSemaphore, nullptr);
    }


    // destroy swapchain resources
    vkDestroySwapchainKHR(m_VkDevice, m_SwapChain, nullptr);
    for (int i = 0; i < m_SwapChainImageViews.size(); i++) {
        vkDestroyImageView(m_VkDevice, m_SwapChainImageViews[i], nullptr);
    }

    vkDestroyDevice(m_VkDevice, nullptr);
    vkDestroySurfaceKHR(m_VkInstance, m_VkSurface, nullptr);
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
    m_MainDeletionQueue.PushFunction([&](){vmaDestroyAllocator(m_Allocator);});

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

    //hardcoding the draw format to 32 bit float
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

    //allocate and create the image
    vmaCreateImage(m_Allocator, &rimg_info, &rimg_allocinfo, &m_DrawImage.image, &m_DrawImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vknatorinit::imageview_create_info(m_DrawImage.imageFormat, m_DrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(m_VkDevice, &rview_info, nullptr, &m_DrawImage.imageView));

    //add to deletion queues
    m_MainDeletionQueue.PushFunction([=]() {
        vkDestroyImageView(m_VkDevice, m_DrawImage.imageView, nullptr);
        vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);
    });
//< init_swap

}

void VknatorEngine::InitCommands(){
    VkCommandPoolCreateInfo cmdPoolInfo = vknatorinit::command_pool_create_info(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++){
        VK_CHECK(vkCreateCommandPool(m_VkDevice, &cmdPoolInfo, nullptr, &m_Frames[i].commandPool));
        VkCommandBufferAllocateInfo cmdBufferAllocInfo = vknatorinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(m_VkDevice, &cmdBufferAllocInfo, &m_Frames[i].mainCommandBuffer));
    }
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
}



