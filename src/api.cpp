#include "api.hpp"

#include "format.hpp"
#include "wavstream.hpp"
#include "stream.hpp"
#include "emitter.hpp"

#include "btime.hpp"

#include <iostream>
#include <algorithm>
#include <atomic>
#include <thread>

#include <math.h>

#include "global.hpp"
#include "wasapi.hpp"
#include "dsound.hpp"

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

float mixbuffer[4096*256];
std::thread mixer_thread;

// Game logic goes between calls to fauxmic_push(). input -> game logic -> fauxmic_push() -> framerate limiter
DLLEXPORT TYPE_VD fauxmix_push()
{
    commandlock.lock();
    copybuffer.insert(copybuffer.end(), cmdbuffer.begin(), cmdbuffer.end());
    cmdbuffer.clear();
    commandlock.unlock();
}
DLLEXPORT TYPE_VD fauxmix_close()
{
    initiated = false;
}

void wrap_mix(float * buffer, uint64_t count, uint64_t freq, uint64_t channels)
{
    commandlock.lock();
    for(auto command : copybuffer)
        command.func();
    copybuffer.clear();
    commandlock.unlock();
    
    mix(buffer, count, freq, channels);
}

// Returns true if device seemed to open correctly
DLLEXPORT TYPE_BL fauxmix_init(TYPE_NM samplerate, TYPE_NM samples)
{
    // FIXME send samplerate, sample count, floatness to init funcs
    if(wasapi_init() == 0)
    {
        puts("Mixer faucet opened WASAPI audio device:");
        mixer_thread = std::thread(wasapi_renderer, wrap_mix, non_time_critical_update_cleanup);
        mixer_thread.detach();
    }
    else if(dsound_init() == 0)
    {
        puts("Mixer faucet opened DirectSound audio device:");
        mixer_thread = std::thread(dsound_renderer, wrap_mix, non_time_critical_update_cleanup);
        mixer_thread.detach();
    }
    else
    {
        puts("Could not open WASAPI or directsound.");
        return false;
    }
    
    initiated = true;
    
    return true;
}
DLLEXPORT TYPE_NM fauxmix_get_samplerate()
{
    if(initiated)
        return 0;
    else
        return -1;
}

DLLEXPORT TYPE_NM fauxmix_get_channels()
{
    if(initiated)
        return 0;
    else
        return -1;
}

DLLEXPORT TYPE_NM fauxmix_get_samples()
{
    if(initiated)
        return 0;
    else
        return -1;
}

// returns whether ducker is doing ducking things (for mastering debugging)
DLLEXPORT TYPE_BL fauxmix_is_ducking()
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
#ifndef NO_OPUS
#include "opusfile.hpp"
#endif
#include "global.hpp"

#include <string.h>
DLLEXPORT TYPE_ID fauxmix_sample_load(TYPE_ST filename)
{
    auto i = sampleids.New();
    
    cmdbuffer.push_back({[i, filename]()
    {
        #ifndef NO_OPUS
            // garbage, rewrite
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
    
    return i;
}
DLLEXPORT TYPE_EC fauxmix_sample_volume(TYPE_ID sample, TYPE_FT volume)
{
    if(sampleids.Exists(sample))
    {
        cmdbuffer.push_back({[sample, volume]()
        {
            auto mine = samples[sample];
            mine->volume = volume;
        }});
        return 0;
    }
    else
        return -1;
}
DLLEXPORT TYPE_VD fauxmix_sample_kill(TYPE_ID sample)
{
    sampleids.Free(sample);
    cmdbuffer.push_back({[sample]()
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
}

DLLEXPORT TYPE_ID fauxmix_emitter_create(TYPE_ID sample)
{
    auto i = emitterids.New();
    cmdbuffer.push_back({[i, sample]()
    {
        if(samples.count(sample) != 0)
        {
            auto s = samples[sample];
            auto mine = new emitter(s);
            emitters.emplace(i, mine);
            samplestoemitters[sample].push_back(i);
        }
    }});
    return i;
}

DLLEXPORT TYPE_ID fauxmix_emitter_create_transient(TYPE_ID sample)
{
    auto i = emitterids.New();
    cmdbuffer.push_back({[i, sample]()
    {
        if(samples.count(sample) != 0)
        {
            auto s = samples[sample];
            auto mine = new emitter(s, true);
            emitters.emplace(i, mine);
            samplestoemitters[sample].push_back(i);
        }
    }});
    return i;
}

DLLEXPORT TYPE_EC fauxmix_emitter_volumes(TYPE_ID id, TYPE_FT left, TYPE_FT right)
{
    float fadewindow = 100;//float(got.freq)*0.01f;
    if(emitterids.Exists(id))
    {
        cmdbuffer.push_back({[id, left, right, fadewindow]()
        {
            if(emitters.count(id) != 0)
            {
                mixinfo* mix = &(emitters[id]->mix);
                if(emitters[id]->info.playing)
                {
                    mix->target_l = left;
                    mix->target_r = right;
                    mix->add_l = (mix->target_l - mix->vol_l)/fadewindow;
                    mix->add_r = (mix->target_r - mix->vol_r)/fadewindow;
                    mix->remaining = ceil(fadewindow);
                }
                else
                {
                    mix->vol_l = left;
                    mix->vol_r = right;
                }
            }
        }});
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_loop(TYPE_ID id, TYPE_BL whether)
{
    if(emitterids.Exists(id))
    {
        cmdbuffer.push_back({[id, whether]()
        {
            if(emitters.count(id) != 0)
            {
                emitters[id]->info.loop = whether;
            }
        }});
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_channel(TYPE_ID id, TYPE_ID channel)
{
    if(emitterids.Exists(id))
    {
        cmdbuffer.push_back({[id, channel]()
        {
            if(emitters.count(id) != 0)
            {
                emitters[id]->mix.channel = channel;
            }
        }});
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_pitch(TYPE_ID id, TYPE_FT ratefactor)
{
    if(emitterids.Exists(id))
    {
        cmdbuffer.push_back({[id, ratefactor]()
        {
            if(emitters.count(id) != 0)
            {
                emitters[id]->info.ratefactor = ratefactor;
            }
        }});
        return 0;
    }
    else
        return -1;
}

DLLEXPORT TYPE_EC fauxmix_emitter_fire(TYPE_ID id)
{
    if(emitterids.Exists(id))
    {
        cmdbuffer.push_back({[id]()
        {
            if(emitters.count(id) != 0)
            {
                auto mine = emitters[id];
                mine->fire();
            }
        }});
        return 0;
    }
    else
        return -1;
}
DLLEXPORT TYPE_EC fauxmix_emitter_cease(TYPE_ID id)
{
    if(emitterids.Exists(id))
    {
        cmdbuffer.push_back({[id]()
        {
            if(emitters.count(id) != 0)
            {
                auto mine = emitters[id];
                mine->cease();
            }
        }});
        return 0;
    }
    else
        return -1;
}
DLLEXPORT TYPE_VD fauxmix_emitter_kill(TYPE_ID id)
{
    emitterids.Free(id);
    cmdbuffer.push_back({[id]()
    {
        if(emitters.count(id) != 0)
        {
            delete emitters[id];
            emitters.erase(id);
            emitterids.Free(id);
        }
    }});
}

DLLEXPORT TYPE_EC fauxmix_sample_status(TYPE_ID sample)
{
    int ret;
    if(sampleshadow.count(sample) != 0)
        ret = sampleshadow[sample].status;
    else
        ret = -2;
    return ret;
}

DLLEXPORT TYPE_EC fauxmix_emitter_status(TYPE_ID id)
{
    int ret;
    if(emittershadow.count(id) != 0)
        ret = emittershadow[id].status;
    else
        ret = -2;
    return ret;
}
