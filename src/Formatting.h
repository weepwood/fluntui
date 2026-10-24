#pragma once
#include <cstdint>
#include <string>

namespace fsi {
std::string humanBytes(std::uint64_t bytes);
std::string humanRate(double bytesPerSecond);
double percentage(std::uint64_t used, std::uint64_t total);
double deltaRate(std::uint64_t current, std::uint64_t previous, double seconds);
int normalizeSamplingInterval(int milliseconds);
int nextSamplingInterval(int milliseconds);
std::string humanUptime(std::uint64_t seconds);
} // namespace fsi
