#pragma once

#include <SDL2/SDL.h>
#include "vknator_types.h"
#include <deque>
#include "vknator_descriptors.h"
#include <vknator_pipelines.h>

constexpr unsigned int FRAME_OVERLAP = 2;

struct DeletionQueue{
    std::deque<std::function<void()>> deletors;

    void PushFunction(std::function<void()>&& function){
        deletors.push_back(function);
    }

    void Flush(){
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++){
            (*it)();
        }
        deletors.clear();
    }
};
struct FrameData {
    VkSemaphore swapchainSemaphore, renderSemaphore;
    VkFence renderFence;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    DeletionQueue deletionQueue;
};

struct ComputePushConstants{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
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
    // ImmediateSubmit
    void ImmediatSubmit(std::function<void(VkCommandBuffer &cmd)>&&function);
    // Draw Imgui
    void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
private:

    void InitVulkan();
    void CreateSwapchain(uint32_t width, uint32_t height);
    void InitSwapchain();
    void InitCommands();
    void InitSyncStructures();
    void InitDescriptors();
    FrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % FRAME_OVERLAP];}
    void DrawBackground(VkCommandBuffer cmd);
    void InitPipelines();
	void InitBackgroundPipelines();
    void InitImGui();

private:
    SDL_Window* m_Window {nullptr};
    VkExtent2D m_WindowExtent{2560, 1440};
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
    DeletionQueue m_MainDeletionQueue;
    VmaAllocator m_Allocator;
    AllocatedImage m_DrawImage;
    VkExtent2D m_DrawExtent;

    DescriptorAllocator m_GlobalDescriptorAllocator;
    VkDescriptorSet m_DrawImageDescriptors;
    VkDescriptorSetLayout m_DrawImageDescriptorLayout;

    VkPipeline m_GradientPipeline;
	VkPipelineLayout m_GradientPipelineLayout;

    //immediate submit structures
    VkFence m_ImmFence;
    VkCommandBuffer m_ImmCommandBuffer;
    VkCommandPool m_ImmCommandPool;


    bool m_IsRunning {true};
    bool m_IsMinimized {false};
    int m_FrameNumber {0};
};
