#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h" // enable proprietary type logging
#include <memory>

namespace vknator{
    class Log{
    public:
        static void Init();
        static std::shared_ptr<spdlog::logger> GetLogger(){ return s_Logger;}
    private:
        static std::shared_ptr<spdlog::logger> s_Logger;
    };
}


#define LOG_INFO(...)			::vknator::Log::GetLogger()->info(__VA_ARGS__)
#define LOG_ERROR(...)			::vknator::Log::GetLogger()->error(__VA_ARGS__)
#define LOG_DEBUG(...)			::vknator::Log::GetLogger()->debug(__VA_ARGS__)
#define LOG_CRITICAL(...)	    ::vknator::Log::GetLogger()->critical(__VA_ARGS__)