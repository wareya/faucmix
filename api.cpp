#include "api.hpp"

#include "format.hpp"
#include "wavstream.hpp"
#include "stream.hpp"
#include "emitter.hpp"
#include "global.hpp"

#include <SDL2/SDL_audio.h>

#define DLLEXPORT extern "C" __attribute__((visibility("default")))

DLLEXPORT void fauxmix_dll_init()
{
    isfloat = false;
    initiated = false;
    volume = 1.0f;
}

DLLEXPORT bool fauxmix_use_float_output(bool b)
{
    isfloat = b;
}

SDL_AudioSpec want;
SDL_AudioSpec got;

DLLEXPORT bool fauxmix_init(int samplerate, bool mono, int samples)
{
    want.freq = samplerate;
    want.format = AUDIO_S16;
    want.channels = channels;
    want.samples = samples;
    want.callback = respondtoSDL;
    want.userdata = &got;
    auto r = SDL_OpenAudio(&want, &got);
    if(r < 0)
    {
        puts("Failed to open device");
        std::cout << SDL_GetError();
        return false;
    }
    
    printf("%d\n", got.freq);
    printf("%d\n", got.samples);
    return true;
}
DLLEXPORT int fauxmix_get_samplerate()
{
    if(initiated)
        return got.samplerate;
    else
        return -1;
}

DLLEXPORT int fauxmix_get_channels()
{
    if(initiated)
        return got.channels;
    else
        return -1;
}

DLLEXPORT int fauxmix_get_samples()
{
    if(initiated)
        return got.samples;
    else
        return -1;
}


 bool fauxmix_is_ducking()

 fauxmix_set_fadetime(milliseconds)
 Default: 5ms
 Limit: 100ms

 fauxmix_set_global_volume(volume)
