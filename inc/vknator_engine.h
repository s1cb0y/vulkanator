#pragma once

#include <SDL2/SDL.h>
#include "vknator_types.h"

namespace vknator{
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
        SDL_Window* m_Window {nullptr};
        VkExtent2D m_WindowSize{1920, 1080};
        bool m_IsRunning {true};
        bool m_IsMinimized {false};
    };

}