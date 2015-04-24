#ifndef INCLUDED_FORMAT
#define INCLUDED_FORMAT

#include <SDL2/SDL_types.h>
#include <SDL2/SDL_audio.h>

#include <atomic>
#include <string>

template <typename type>
Uint64 ipower(type b, type n)
{
    if(n == 0)
        return 1;
    if(n < 0)
        return 1/ipower(b,-n);
    Uint64 x = b;
    for(auto i = 1; i < n; i++)
        x *= b;
    return x;
}

struct emitterinfo
{
    float pan;
    float volume;
    bool loop;
    bool playing;
    float mixdown;
};

struct wavformat
{
    bool isfloatingpoint;
    Uint16 channels;
    Uint32 samplerate;
    Uint16 blocksize;
    Uint8 bytespersample;
    float datagain;
    double slowdatagain;
};

struct wavfile
{
    std::atomic<bool> ready;
    
    Uint8 * fmt = nullptr;
    Uint8 * data = nullptr;
    Uint32 samples;
    Uint32 bytes;
    
    std::string stored;
    wavformat format;
};

wavformat audiospec_to_wavformat(SDL_AudioSpec * from);
float get_sample(void * addr, wavformat * fmt);
void set_sample(Uint8 * addr, wavformat * fmt, float val);

#endif
