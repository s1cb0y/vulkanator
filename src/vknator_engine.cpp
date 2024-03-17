#include "vknator_engine.h"
#include "vknator_log.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <VkBootstrap.h>

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

namespace vknator
{
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
                                        m_WindowSize.width,
                                        m_WindowSize.height,
                                        SDL_WINDOW_VULKAN);
        if (m_Window == NULL){
            LOG_ERROR("Error window creation");
            success = false;
        }

        InitVulkan();

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
        //TODO implement
    }

    void VknatorEngine::Deinit(){
        LOG_DEBUG("Shut down engine...");

        //wait for GPU to stop
        vkDeviceWaitIdle(m_VkDevice);
        // destroy command pools, which destroy all allocated command buffers
        for (int i = 0; i < FRAME_OVERLAP; i++){
            vkDestroyCommandPool(m_VkDevice, m_Frames[i].commandPool, nullptr);
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

        InitSwapchain();
        m_GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        m_GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
        InitCommands();
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
        CreateSwapchain(m_WindowSize.width, m_WindowSize.height);
    }

    void VknatorEngine::InitCommands(){
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.pNext = nullptr;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = m_GraphicsQueueFamily;

        for (int i = 0; i < FRAME_OVERLAP; i++){
            VK_CHECK(vkCreateCommandPool(m_VkDevice, &cmdPoolInfo, nullptr, &m_Frames[i].commandPool));
            VkCommandBufferAllocateInfo cmdBufferAllocInfo{};
            cmdBufferAllocInfo.commandPool = m_Frames[i].commandPool;
            cmdBufferAllocInfo.commandBufferCount = 1;
            cmdBufferAllocInfo.pNext = nullptr;
            cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            VK_CHECK(vkAllocateCommandBuffers(m_VkDevice, &cmdBufferAllocInfo, &m_Frames[i].mainCommandBuffer));
        }
    }


} // namespace vknator


