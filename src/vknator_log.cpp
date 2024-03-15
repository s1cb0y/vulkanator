#include "vknator_log.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace vknator{

    std::shared_ptr<spdlog::logger> Log::s_Logger;

    void Log::Init(){
        s_Logger = spdlog::stdout_color_mt("vknator");
        spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    }
}