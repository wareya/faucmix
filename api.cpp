#include "api.hpp"

#include "format.hpp"
#include "wavstream.hpp"
#include "stream.hpp"
#include "emitter.hpp"

#include "btime.hpp"

#include <SDL2/SDL_audio.h>

#include <iostream>
#include <math.h>
#include <algorithm>

#include "global.hpp"
bool isfloat;

DLLEXPORT TYPE_VD fauxmix_dll_init()
{
    initiated = false;
    volume = 1.0f;
    ducker = 1.0f;
    isfloat = false;
}

DLLEXPORT TYPE_VD fauxmix_use_float_output(TYPE_BL b)
{
    isfloat = b;
}

SDL_AudioSpec want;
SDL_AudioSpec got;

DLLEXPORT TYPE_BL fauxmix_init(TYPE_NM samplerate, TYPE_BL mono, TYPE_NM samples)
{
    want.freq = samplerate;
    if(!isfloat)
        want.format = AUDIO_S16;
    else
        want.format = AUDIO_F32;
    want.channels = (mono < 0.5)?1:2;
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
DLLEXPORT TYPE_NM fauxmix_get_samplerate()
{
    if(initiated)
        return got.freq;
    else
        return -1;
}

DLLEXPORT TYPE_NM fauxmix_get_channels()
{
    if(initiated)
        return got.channels;
    else
        return -1;
}

DLLEXPORT TYPE_NM fauxmix_get_samples()
{
    if(initiated)
        return got.samples;
    else
        return -1;
}

DLLEXPORT TYPE_BL fauxmix_is_ducking()
{
    return ducker > 1.0f;
}

DLLEXPORT TYPE_EC fauxmix_channel(TYPE_ID id, TYPE_FT volume)
{
    commandlock.lock();
        cmdbuffer.push_back({get_us(), [id, volume]()
        {
            if(mixchannels.count(id) == 0 and id >= 0)
            {
                mixchannels[id] = (volume > 1.0f ? 1.0f : volume);
            }
        }});
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
#ifndef NO_OPUS
#include "opusfile.hpp"
#endif
#include "global.hpp"

#include <string.h>
DLLEXPORT TYPE_ID fauxmix_sample_load(TYPE_ST filename)
{
    auto i = sampleids.New();
    
    commandlock.lock();
        cmdbuffer.push_back({get_us(), [i, filename]()
        {
            #ifndef NO_OPUS
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
            #endif
            wav:
            {
                auto n = wavfile_load(filename);
                samples.emplace(i, n);
                return;
            }
        }});
    commandlock.unlock();
    
    return i;
}
DLLEXPORT TYPE_EC fauxmix_sample_volume(TYPE_ID sample, TYPE_FT volume)
{
    commandlock.lock();
        if(samples.count(sample) != 0)
        {
            cmdbuffer.push_back({get_us(), [sample, volume]()
            {
                auto mine = samples[sample];
                mine->volume = volume;
            }});
            return 0;
        }
        else
            return -1;
    commandlock.unlock();
}
DLLEXPORT TYPE_VD fauxmix_sample_kill(TYPE_ID sample)
{
    sampleids.Free(sample);
    commandlock.lock();
        cmdbuffer.push_back({get_us(), [sample]()
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
        }});
    commandlock.unlock();
}
DLLEXPORT TYPE_EC fauxmix_sample_status(TYPE_ID sample)
{
    int ret;
    shadowlock.lock();
        if(sampleshadow.count(sample) != 0)
            ret = sampleshadow[sample].status;
        else
            ret = -2;
    shadowlock.unlock();
    return ret;
}



DLLEXPORT TYPE_ID fauxmix_emitter_create(TYPE_ID sample)
{
    auto i = emitterids.New();
    commandlock.lock();
        cmdbuffer.push_back({get_us(), [i, sample]()
        {
            if(samples.count(sample) != 0)
            {
                auto s = samples[sample];
                auto mine = new emitter(s);
                emitters.emplace(i, mine);
                samplestoemitters[sample].push_back(i);
            }
        }});
    commandlock.unlock();
    return i;
}
 
DLLEXPORT TYPE_EC fauxmix_emitter_status(TYPE_ID id)
{
    int ret;
    shadowlock.lock();
        if(emittershadow.count(id) != 0)
            ret = emittershadow[id].status;
        else
            ret = -2;
    shadowlock.unlock();
    return ret;
}

DLLEXPORT TYPE_EC fauxmix_emitter_volumes(TYPE_ID id, TYPE_FT left, TYPE_FT right)
{
    float fadewindow = float(got.freq)*0.01f;
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back({get_us(), [id, left, right, fadewindow]()
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
            }});
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_loop(TYPE_ID id, TYPE_BL whether)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back({get_us(), [id, whether]()
            {
                if(emitters.count(id) != 0)
                {
                    emitters[id]->info.loop = whether;
                }
            }});
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_channel(TYPE_ID id, TYPE_ID channel)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back({get_us(), [id, channel]()
            {
                if(emitters.count(id) != 0)
                {
                    emitters[id]->mix.channel = channel;
                }
            }});
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_pitch(TYPE_ID id, TYPE_FT ratefactor)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back({get_us(), [id, ratefactor]()
            {
                if(emitters.count(id) != 0)
                {
                    emitters[id]->info.ratefactor = ratefactor;
                }
            }});
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_fire(TYPE_ID id)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back({get_us(), [id]()
            {
                if(emitters.count(id) != 0)
                {
                    auto mine = emitters[id];
                    mine->fire();
                }
            }});
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}
DLLEXPORT TYPE_EC fauxmix_emitter_cease(TYPE_ID id)
{
    if(emitterids.Exists(id))
    {
        commandlock.lock();
            cmdbuffer.push_back({get_us(), [id]()
            {
                if(emitters.count(id) != 0)
                {
                    auto mine = emitters[id];
                    mine->cease();
                }
            }});
        commandlock.unlock();
        return 0;
    }
    else
        return -1;
}
DLLEXPORT TYPE_VD fauxmix_emitter_kill(TYPE_ID id)
{
    emitterids.Free(id);
    commandlock.lock();
        cmdbuffer.push_back({get_us(), [id]()
        {
            if(emitters.count(id) != 0)
            {
                delete emitters[id];
                emitters.erase(id);
                emitterids.Free(id);
            }
        }});
    commandlock.unlock();
}
