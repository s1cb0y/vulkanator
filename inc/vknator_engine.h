#pragma once

#include <SDL2/SDL.h>
#include "vknator_types.h"
#include <deque>
#include "vknator_descriptors.h"
#include <vknator_pipelines.h>
#include <vknator_loader.h>

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
    DescriptorAllocatorGrowable frameDescriptors;
};

struct ComputePushConstants{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect{
    const char* name;
    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

struct GPUSceneData{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
};

struct GLTFMetallic_Roughness{
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants{
        glm::vec4 colorFactors;
        glm::vec4 metal_rough_factors;
        //padding, we need it anyway for uniform buffers (GPUs require minimum alignment size of 256 bytes)
        glm::vec4 extra[14];
    };

    struct MaterialResources{
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    void BuildPipelines(VknatorEngine* engine);
    void ClearResources(VkDevice device);

    MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
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
    void ImmediateSubmit(std::function<void(VkCommandBuffer &cmd)>&&function);
    // Draw Imgui
    void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    // Draw geometry
    void DrawGeometry(VkCommandBuffer cmd);

    GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
public:
    VkDevice m_VkDevice;
    VkDescriptorSetLayout m_GPUSceneDataDescriptorSetLayout;
    AllocatedImage m_DrawImage;
    AllocatedImage m_DepthImage;

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
    void InitMeshPipeline();
    void InitImGui();
    void InitDefaultData();
    AllocatedBuffer CreateBuffer(std::size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void DestroyBuffer(const AllocatedBuffer& buffer);
    void DestroySwapchain();
    void ResizeSwapchain();
    AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void DestroyImage(const AllocatedImage& img);


private:
    SDL_Window* m_Window {nullptr};
    VkExtent2D m_WindowExtent{1700 , 900};
    VkInstance m_VkInstance;
    VkSurfaceKHR m_VkSurface;
    VkDebugUtilsMessengerEXT m_VkDebugMessenger;
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
    VkExtent2D m_DrawExtent;

    DescriptorAllocatorGrowable m_GlobalDescriptorAllocator;
    VkDescriptorSet m_DrawImageDescriptors;
    VkDescriptorSetLayout m_DrawImageDescriptorLayout;
    /* pipelines */
    VkPipeline m_GradientPipeline;
	VkPipelineLayout m_GradientPipelineLayout;

    VkPipeline m_MeshPipeline;
    VkPipelineLayout m_MeshPipelineLayout;

    //immediate submit structures
    VkFence m_ImmFence;
    VkCommandBuffer m_ImmCommandBuffer;
    VkCommandPool m_ImmCommandPool;

    //compute effects
    std::vector<ComputeEffect> m_BackgroundEffects;

    //scene data
    GPUSceneData m_SceneData;
    // materials
    MaterialInstance m_DefaultData;
    GLTFMetallic_Roughness m_MetalRoughMaterial;

    //textures
    AllocatedImage m_WhiteImage;
    AllocatedImage m_BlackImage;
    AllocatedImage m_GreyImage;
    AllocatedImage m_ErrorCheckerboardImage;

    VkSampler m_DefaultSamplerLinear;
    VkSampler m_DefaultSamplerNearest;

    VkDescriptorSetLayout m_SingleImageDescriptorLayout;

    bool m_ResizeRequested {false};

    int m_CurrentBackgroundEffect{0};
    bool m_IsRunning {true};
    bool m_IsMinimized {false};
    int m_FrameNumber {0};
    std::vector<std::shared_ptr<MeshAsset>> m_testMeshes;
    float m_RenderScale{1.f};
};
