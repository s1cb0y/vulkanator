#pragma once

#include <SDL2/SDL.h>
#include "vknator_types.h"

namespace vknator{

    constexpr unsigned int FRAME_OVERLAP = 2;

    struct FrameData {
        VkSemaphore swapchainSemaphore, renderSemaphore;
        VkFence renderFence;

        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;
    };

    class VknatorEngine{
    public:
        //init engine
        bool Init();
        // deinit engine resource
        void Deinit();
        //run main loop
        void Run();
        // Draw
        void Draw();

    private:

        void InitVulkan();
        void CreateSwapchain(uint32_t width, uint32_t height);
        void InitSwapchain();
        void InitCommands();
        void InitSyncStructures();
        FrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % FRAME_OVERLAP];}

    private:
        SDL_Window* m_Window {nullptr};
        VkExtent2D m_WindowSize{1920, 1080};
        VkInstance m_VkInstance;
        VkSurfaceKHR m_VkSurface;
        VkDebugUtilsMessengerEXT m_VkDebugMessenger;
        VkDevice m_VkDevice;
        VkPhysicalDevice m_ActiveGPU;
        VkSwapchainKHR m_SwapChain;
        VkFormat m_SwapChainImageFormat;
        VkExtent2D m_SwapChainExtent;
        std::vector<VkImage> m_SwapChainImages;
        std::vector<VkImageView> m_SwapChainImageViews;
        VkQueue m_GraphicsQueue;
        uint8_t m_GraphicsQueueFamily;
        FrameData m_Frames[FRAME_OVERLAP];

        bool m_IsRunning {true};
        bool m_IsMinimized {false};
        int m_FrameNumber {0};
    };

}