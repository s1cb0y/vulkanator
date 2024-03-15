#include "vknator_engine.h"
#include <SDL2/SDL.h>
#include <iostream>

namespace vknator
{
    bool VknatorEngine::Init(){
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            std::cout << "Error SDL2 Initialization : " << SDL_GetError();
            return false;
        }
        m_Window = SDL_CreateWindow("VKnator Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_WindowSize.width, m_WindowSize.height, SDL_WINDOW_SHOWN);
        return true;
    }

    void VknatorEngine::Run(){
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_QUIT){
                break;
            }
        }
    }

    void VknatorEngine::Deinit(){
        //Destroy window
        SDL_DestroyWindow( m_Window );
        m_Window = NULL;

        //Quit SDL subsystems
        SDL_Quit();
    }


} // namespace vknator


