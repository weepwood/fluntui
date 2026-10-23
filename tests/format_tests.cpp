#include "Formatting.h"
#include <cstdlib>
#include <string>
#include <cmath>
#include <iostream>
#include <limits>

void check(bool condition, const char* expression) {
    if (!condition) {
        std::cerr << "FAILED: " << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}
#define CHECK(expr) check((expr), #expr)

int main() {
    using namespace fsi;
    CHECK(humanBytes(0) == "0 B");
    CHECK(humanBytes(1024) == "1.0 KiB");
    CHECK(humanBytes(1073741824) == "1.0 GiB");
    CHECK(humanRate(0) == "0 B/s");
    CHECK(humanRate(std::numeric_limits<double>::quiet_NaN()) == "0 B/s");
    CHECK(percentage(45, 100) == 45.0);
    CHECK(percentage(1, 0) == 0.0);
    CHECK(percentage(200, 100) == 100.0);
    CHECK(deltaRate(1100, 1000, 2.0) == 50.0);
    CHECK(deltaRate(900, 1000, 1.0) == 0.0);
    CHECK(deltaRate(1100, 1000, 0.0) == 0.0);
    CHECK(humanUptime(90061) == "1d 1h 1m");
    std::cout << "Formatting tests passed\n";
}
