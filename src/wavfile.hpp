#ifndef INCLUDED_WAVFLIE
#define INCLUDED_WAVFLIE

#include "format.hpp"

#include <atomic>
#include <string>

struct wavfile
{
    std::atomic<int> status;
    
    Uint8 * fmt = nullptr;
    Uint8 * data = nullptr;
    Uint32 samples;
    Uint32 bytes;
    
    std::string stored;
    wavformat format;
};

wavfile * wavfile_load (const char * filename);

int t_wavfile_load(void * etc);

#endif
