#include "vknator_engine.h"
#include "vknator_log.h"
#include <SDL2/SDL.h>
#include <VkBootstrap.h>

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

        success ? LOG_DEBUG("Done") : LOG_DEBUG("Failed");
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
        //Destroy window
        SDL_DestroyWindow( m_Window );
        m_Window = NULL;

        //Quit SDL subsystems
        SDL_Quit();
        LOG_DEBUG("Done");
    }

    void VknatorEngine::InitVulkan(){
        vkb::InstanceBuilder builder;

    }


} // namespace vknator


