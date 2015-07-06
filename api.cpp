#include "api.hpp"

#include "format.hpp"
#include "wavstream.hpp"
#include "stream.hpp"
#include "emitter.hpp"

#include <SDL2/SDL_audio.h>

#include <iostream>
#include <math.h>
#include <algorithm>

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

DLLEXPORT int fauxmix_channel(Uint32 id, float volume)
{
    commandlock.lock();
        cmdbuffer.push_back([id, volume]()
        {
            if(mixchannels.count(id) == 0 and id >= 0)
            {
                mixchannels[id] = (volume > 1.0f ? 1.0f : volume);
            }
        });
    commandlock.unlock();
    return 0;
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
#include "opusfile.hpp"
#include "global.hpp"

#include <string.h>
DLLEXPORT Uint32 fauxmix_sample_load(const char * filename)
{
    auto i = sampleids.New();
    
    commandlock.lock();
        cmdbuffer.push_back([i, filename]()
        {
            auto b = strlen(filename);
            int a;
            if(b > 10000) // no
            {
                puts("Long filename -- cancelling opus detection!");
                goto wav; // this has to be controlled for overflow of b - 5
            }
            a = b - 5;
            if(a < 0)
            {
                puts("Short filename -- not opus!");
                goto wav; // this has to be controlled for possible strcmp exploits
            }
            if(strncmp(".opus", filename+a, 5) != 0)
            {
                puts("No overlap -- not opus!");
                std::cout << filename+a << "\n";
                goto wav;
            }
            opus:
            {
                auto n = opusfile_load(filename);
                samples.emplace(i, n);
                return;
            }
            wav:
            {
                auto n = wavfile_load(filename);
                samples.emplace(i, n);
                return;
            }
        });
    commandlock.unlock();
    
    return i;
}
DLLEXPORT int fauxmix_sample_volume(Uint32 sample, float volume)
{
    commandlock.lock();
        if(samples.count(sample) != 0)
        {
            cmdbuffer.push_back([sample, volume]()
            {
                auto mine = samples[sample];
                mine->volume = volume;
            });
            return 0;
        }
        else
            return -1;
    commandlock.unlock();
}
DLLEXPORT void fauxmix_sample_kill(Uint32 sample)
{
    sampleids.Free(sample);
    commandlock.lock();
        cmdbuffer.push_back([sample]()
        {
            if(samples.count(sample) != 0)
            {
                delete samples[sample];
                samples.erase(sample);
            }
            if(samplestoemitters.count(sample) != 0)
            {
                for(auto e : samplestoemitters[sample])
                {
                    if(emitters.count(e) != 0)
                    {
                        delete emitters[e];
                        emitters.erase(e);
                        emitterids.Free(e);
                    }
                }
                samplestoemitters.erase(sample);
            }
        });
    commandlock.unlock();
}
DLLEXPORT int fauxmix_sample_status(Uint32 sample)
{
    if(sampleshadow.count(sample) != 0)
    {
        return sampleshadow[sample].status;
    }
    else
        return -2;
}



DLLEXPORT Uint32 fauxmix_emitter_create(Uint32 sample)
{
    auto i = emitterids.New();
    commandlock.lock();
        cmdbuffer.push_back([i, sample]()
        {
            if(samples.count(sample) != 0)
            {
                auto s = samples[sample];
                auto mine = new emitter(s);
                emitters.emplace(i, mine);
                samplestoemitters[sample].push_back(i);
            }
        });
    commandlock.unlock();
    return i;
}
 
DLLEXPORT int fauxmix_emitter_status(Uint32 id)
{
    if(emittershadow.count(id) != 0)
    {
        return emittershadow[id].status;
    }
    else
        return -2;
}

DLLEXPORT int fauxmix_emitter_volumes(Uint32 id, float left, float right)
{
    float fadewindow = float(got.freq)*0.01f;
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back([id, left, right, fadewindow]()
            {
                if(emitters.count(id) != 0)
                {
                    mixinfo* mix = &(emitters[id]->mix);
                    mix->target_l = left;
                    mix->target_r = right;
                    mix->add_l = (mix->target_l - mix->vol_l)/fadewindow;
                    mix->add_r = (mix->target_r - mix->vol_r)/fadewindow;
                    mix->remaining = ceil(fadewindow);
                    //std::cout << mix->target_l << " " << mix->vol_l << " " << (mix->target_l - mix->vol_l) << "\n";
                }
            });
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT int fauxmix_emitter_loop(Uint32 id, bool whether)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back([id, whether]()
            {
                if(emitters.count(id) != 0)
                {
                    emitters[id]->info.loop = whether;
                }
            });
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT int fauxmix_emitter_channel(Uint32 id, int channel)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back([id, channel]()
            {
                if(emitters.count(id) != 0)
                {
                    emitters[id]->mix.channel = channel;
                }
            });
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT int fauxmix_emitter_pitch(Uint32 id, float ratefactor)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back([id, ratefactor]()
            {
                if(emitters.count(id) != 0)
                {
                    emitters[id]->info.ratefactor = ratefactor;
                }
            });
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT int fauxmix_emitter_fire(Uint32 id)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back([id]()
            {
                if(emitters.count(id) != 0)
                {
                    auto mine = emitters[id];
                    mine->fire();
                }
            });
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}
DLLEXPORT int fauxmix_emitter_cease(Uint32 id)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back([id]()
            {
                if(emitters.count(id) != 0)
                {
                    auto mine = emitters[id];
                    mine->cease();
                }
            });
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}
DLLEXPORT void fauxmix_emitter_kill(Uint32 id)
{
    emitterids.Free(id);
    commandlock.lock();
        cmdbuffer.push_back([id]()
        {
            if(emitters.count(id) != 0)
            {
                delete emitters[id];
                emitters.erase(id);
                emitterids.Free(id);
            }
        });
    commandlock.unlock();
}
