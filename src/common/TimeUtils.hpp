#pragma once

#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace TimeUtils {

    /**
     * Парсит PostgreSQL timestamp (например, "2024-07-14 12:34:56")
     * в std::chrono::system_clock::time_point
     * Предполагается, что timestamp хранится в UTC.
     */
    inline std::chrono::system_clock::time_point parse_pg_timestamp(const std::string &s) {
        std::tm tm = {};
        std::istringstream ss(s);

        // Формат PostgreSQL: "YYYY-MM-DD HH:MI:SS"
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            throw std::runtime_error("Failed to parse timestamp: " + s);
        }

        // Преобразуем tm в time_t — UTC.
        // NB: timegm — GNU расширение. Если нет, используй boost::date_time или cctz.
#ifdef _WIN32
        // Windows не имеет timegm по умолчанию — костыль:
        _putenv_s("TZ", "UTC");
        _tzset();
        std::time_t tt = mktime(&tm);
#else
        std::time_t tt = timegm(&tm);
#endif

        return std::chrono::system_clock::from_time_t(tt);
    }

}
