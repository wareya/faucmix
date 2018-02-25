#ifndef INCLUDED_FORMAT
#define INCLUDED_FORMAT

#include <atomic>
#include <string>

template <typename type>
uint64_t ipower(type b, type n)
{
    if(n == 0)
        return 1;
    if(n < 0)
        return 1/ipower(b,-n);
    uint64_t x = b;
    for(auto i = 1; i < n; i++)
        x *= b;
    return x;
}

struct emitterinfo
{
    std::atomic<bool> playing;
    bool loop = false;
    float ratefactor = 1.0f;
};

struct wavformat
{
    bool isfloatingpoint;
    uint16_t channels;
    uint32_t samplerate;
    uint16_t blocksize;
    uint8_t bytespersample;
    float datagain;
    double slowdatagain;
    float volume;
};

#endif
