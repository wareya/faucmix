#include "api.hpp"

#include "format.hpp"
#include "wavstream.hpp"
#include "stream.hpp"
#include "emitter.hpp"

#include <SDL2/SDL_audio.h>

#include <iostream>

#include "global.hpp"
bool isfloat;

DLLEXPORT void fauxmix_dll_init()
{
    initiated = false;
    volume = 1.0f;
    ducker = 1.0f;
    isfloat = false;
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
    if(!isfloat)
        want.format = AUDIO_S16;
    else
        want.format = AUDIO_F32;
    want.channels = 2;
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
    SDL_PauseAudio(0);
    
    initiated = true;
    printf("%d\n", got.freq);
    printf("%d\n", got.samples);
    
    return true;
}
DLLEXPORT int fauxmix_get_samplerate()
{
    if(initiated)
        return got.freq;
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

DLLEXPORT bool fauxmix_is_ducking()
{
    return ducker > 1.0f;
}

/*
 * Samples have to be loaded on a thread or else client game logic would cause synchronous disk IO
 * However, this means that the "load sample" command can't give a definitive response on the validity of a sample file
 * It has to be checked in some other way
 * The idea I just had is:
 * Sample handles can be loaded, loading, unloaded, or failed
 * and you can poll their status
 * and it's up to you to kill them if they fail or unload
 */
#include "wavfile.hpp"
DLLEXPORT wavfile * fauxmix_sample_load(const char * filename)
{
    return wavfile_load(filename);
}
DLLEXPORT void fauxmix_sample_volume(void * sample, float volume)
{
    auto mine = (wavfile*)sample;
    mine->volume = volume;
}
DLLEXPORT void fauxmix_sample_kill(void * sample)
{
    // ????? TODO; HAVE TO HANDLE WEAKNESS IN EMITTER
}
DLLEXPORT int fauxmix_sample_status(void * sample)
{
    auto mine = (wavfile*)sample;
    return mine->status;
}



DLLEXPORT emitter * fauxmix_emitter_create(void * sample)
{
    auto mine = new emitter((wavfile*)sample);
    emitters.insert(mine);
    return mine;
}
 
DLLEXPORT wavfile * fauxmix_emitter_sample(emitter * mine)
{
    return ((wavstream *)(mine->stream))->sample; // TODO: check for wavstream before casting
}
 
DLLEXPORT int fauxmix_emitter_status(emitter * mine)
{
    return fauxmix_sample_status(((wavstream *)(mine->stream))->sample); // TODO: check for wavstream before casting
}

//fauxmix_emitter_volumes(emitter, left, right)

DLLEXPORT void fauxmix_emitter_fire(emitter * mine)
{
    mine->fire();
}
DLLEXPORT void fauxmix_emitter_cease(emitter * mine)
{
    mine->cease();
}
DLLEXPORT void fauxmix_emitter_kill(emitter * mine)
{
    emitters.erase(mine);
    delete mine;
}
