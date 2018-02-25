#ifndef INCLUDED_WAVFLIE
#define INCLUDED_WAVFLIE

#include "format.hpp"

#include <atomic>
#include <string>

struct wavfile
{
    std::atomic<int> status;
    
    float * buffer = nullptr;
    float volume = 1.0;
    uint64_t samples = 0;
    uint64_t samplerate = 0;
    uint64_t channels = 0;
    
    std::string stored;
    
    int32_t error;
};

wavfile * wavfile_load (const char * filename);

#endif
