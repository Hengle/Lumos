#include "Precompiled.h"
#include "LMLog.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Lumos::Debug
{
#if LUMOS_ENABLE_LOG
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::vector<spdlog::sink_ptr> sinks;
#endif

    void Log::OnInit()
    {
#if LUMOS_ENABLE_LOG
        sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>()); // debug
                                                                                     // sinks.emplace_back(std::make_shared<ImGuiConsoleSink_mt>()); // ImGuiConsole

#ifdef LOG_TO_TEXT_FILE
        auto logFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("LumosLog.txt", 1048576 * 5, 3);
        sinks.emplace_back(logFileSink); // Log file
#endif

        // create the loggers
        s_CoreLogger = std::make_shared<spdlog::logger>("Lumos", begin(sinks), end(sinks));
        spdlog::register_logger(s_CoreLogger);

        // configure the loggers
        spdlog::set_pattern("%^[%T] %v%$");
        s_CoreLogger->set_level(spdlog::level::trace);
#endif
    }

#if LUMOS_ENABLE_LOG
    void Log::AddSink(spdlog::sink_ptr& sink)
    {
        s_CoreLogger->sinks().push_back(sink);
        s_CoreLogger->set_pattern("%v%$");
    }
#endif

    void Log::OnRelease()
    {
#if LUMOS_ENABLE_LOG
        s_CoreLogger.reset();
        spdlog::shutdown();
#endif
    }
}
