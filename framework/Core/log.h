#pragma once
#include <memory>
#include "spdlog.h"


class Log
{
public:
    static void Init();

    inline static std::shared_ptr<spdlog::logger>& GetCoreLogger()
    {
        return s_core_logger;
    }
    inline static std::shared_ptr<spdlog::logger>& GetClientLogger()
    {
        return s_client_logger;
    }
private:
    static std::shared_ptr<spdlog::logger> s_core_logger;
    static std::shared_ptr<spdlog::logger> s_client_logger;
};

// Core log macros
#define FW_CORE_TRACE(...)Log::GetCoreLogger()->trace(__VA_ARGS__)
#define FW_CORE_INFO(...)Log::GetCoreLogger()->info(__VA_ARGS__)
#define FW_CORE_WARN(...)Log::GetCoreLogger()->warn(__VA_ARGS__)
#define FW_CORE_DEBUG(...)Log::GetCoreLogger()->debug(__VA_ARGS__)
#define FW_CORE_ERROR(...)Log::GetCoreLogger()->error(__VA_ARGS__)
#define FW_CORE_CRITICAL(...)Log::GetCoreLogger()->critical(__VA_ARGS__)

//Client log macros
#define FW_TRACE(...)Log::GetClientLogger()->trace(__VA_ARGS__)
#define FW_INFO(...)Log::GetClientLogger()->info(__VA_ARGS__)
#define FW_WARN(...)Log::GetClientLogger()->warn(__VA_ARGS__)
#define FW_DEBUG(...)Log::GetClientLogger()->debug(__VA_ARGS__)
#define FW_ERROR(...)Log::GetClientLogger()->error(__VA_ARGS__)
#define FW_CRITICAL(...)Log::GetClientLogger()->critical(__VA_ARGS__)

