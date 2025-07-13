#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>

#include <memory>
#include <vector>

class Logger {
public:
    static std::shared_ptr<spdlog::logger> get() {
        static std::shared_ptr<spdlog::logger> logger = create_logger();
        return logger;
    }

    static void init_thread_pool() {
        // Размер очереди и число потоков для фоновой записи
        constexpr size_t queue_size = 8192;
        constexpr size_t num_threads = 1;
        spdlog::init_thread_pool(queue_size, num_threads);
    }

private:
    static std::shared_ptr<spdlog::logger> create_logger() {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    "logs/server.log", 1024 * 1024 * 5, 3);
            file_sink->set_level(spdlog::level::trace);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

            auto logger = std::make_shared<spdlog::async_logger>(
                    "async_logger",
                    sinks.begin(),
                    sinks.end(),
                    spdlog::thread_pool(),
                    spdlog::async_overflow_policy::block
            );

            logger->set_level(spdlog::level::trace);
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

            spdlog::register_logger(logger);
            return logger;

        } catch (const spdlog::spdlog_ex &ex) {
            std::cerr << "Logger init failed: " << ex.what() << std::endl;
            throw;
        }
    }
};
