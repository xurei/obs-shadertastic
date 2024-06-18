#include <chrono>

unsigned long get_time_ms() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);

    auto value = now_ms.time_since_epoch();
    unsigned long duration = (unsigned long)value.count();

    return duration;
}