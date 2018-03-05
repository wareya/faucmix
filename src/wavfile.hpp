#ifndef INCLUDED_WAVFLIE
#define INCLUDED_WAVFLIE

#include "format.hpp"
#include "wavbuffer.hpp"

#include <atomic>
#include <string>

struct wavfile
{
    std::atomic<int> status;
    
    wavbuffer buffer;
    uint64_t samplerate = 0;
    float volume = 1.0;
    
    std::string stored;
    
    int32_t error;
};

wavfile * wavfile_load (const char * filename);

#endif
