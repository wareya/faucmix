#define USE_SOURCE_HEADER_BTIME

#include <chrono>
#include <vector>

std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();
std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();

std::chrono::high_resolution_clock myclock;

double get_us()
{
    end = myclock.now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}
