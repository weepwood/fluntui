#include "Formatting.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace fsi {
std::string humanBytes(std::uint64_t bytes) {
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 6) { value /= 1024.0; ++unit; }
    std::ostringstream result;
    result << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << ' ' << units[unit];
    return result.str();
}
std::string humanRate(double bytesPerSecond) {
    if (!std::isfinite(bytesPerSecond) || bytesPerSecond <= 0) return "0 B/s";
    return humanBytes(static_cast<std::uint64_t>(bytesPerSecond)) + "/s";
}
double percentage(std::uint64_t used, std::uint64_t total) {
    if (total == 0) return 0.0;
    return std::clamp(static_cast<double>(used) * 100.0 / static_cast<double>(total), 0.0, 100.0);
}
double deltaRate(std::uint64_t current, std::uint64_t previous, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0 || current < previous) return 0.0;
    return static_cast<double>(current - previous) / seconds;
}
std::string humanUptime(std::uint64_t seconds) {
    const std::uint64_t days = seconds / 86400;
    const std::uint64_t hours = seconds % 86400 / 3600;
    const std::uint64_t minutes = seconds % 3600 / 60;
    std::ostringstream result;
    if (days) result << days << "d ";
    result << hours << "h " << minutes << "m";
    return result.str();
}
} // namespace fsi
