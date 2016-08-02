#include "api.hpp"

#include "format.hpp"
#include "wavstream.hpp"
#include "stream.hpp"
#include "emitter.hpp"

#include "btime.hpp"

#include <SDL2/SDL_audio.h>

#include <iostream>
#include <algorithm>
#include <atomic>

#include <math.h>

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
volatile SDL_AudioDeviceID device = 0;

unsigned char buffer[4096*256]; // more than enough samples for everyone

std::atomic<SDL_Thread *> mixer_thread;
int pseudo_callback(void * data)
{
    while(device)
    {
        // sdl why are you so bad at the one job you had
        int32_t bytes = 0;
        while(bytes <= 0)
        {
            if(!device)
                goto out;
            bytes = got.size - SDL_GetQueuedAudioSize(device);
            if(bytes <= 0)
                SDL_Delay(1);
        }
        
        commandlock.lock();
        for(auto command : copybuffer)
            command.func();
        copybuffer.clear();
        commandlock.unlock();
        
        if(bytes > 4096*256) bytes = 4096*256;
        respondtoSDL(&got, buffer, bytes);
        SDL_QueueAudio(device, buffer, bytes);
        
        non_time_critical_update_cleanup();
    }
    out:
    mixer_thread = nullptr;
}

// ... game logic goes between calls ...
DLLEXPORT TYPE_VD fauxmix_push()
{
    commandlock.lock();
    copybuffer.insert(copybuffer.end(), cmdbuffer.begin(), cmdbuffer.end());
    cmdbuffer.clear();
    commandlock.unlock();
}
DLLEXPORT TYPE_VD fauxmix_close()
{
    if(device) SDL_CloseAudioDevice(device);
    device = 0;
    
    initiated = false;
}
// Returns true if device seemed to open correctly
DLLEXPORT TYPE_BL fauxmix_init(TYPE_NM samplerate, TYPE_BL mono, TYPE_NM samples)
{
    want.freq = samplerate;
    if(!isfloat)
        want.format = AUDIO_S16;
    else
        want.format = AUDIO_F32;
    // We test whether mono is less than 0.5 just in case we're being used from game maker, where that is the "truth" convention
    want.channels = (mono < 0.5)?1:2;
    want.samples = roundf(powf(2,roundf(log2f(samples))));
    want.callback = 0;
    want.userdata = &got;
    device = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    
    if(device == 0)
    {
        puts("Failed to open device");
        std::cout << SDL_GetError();
        return false;
    }
    
    if(mixer_thread) SDL_WaitThread(mixer_thread, nullptr);
    mixer_thread = SDL_CreateThread(pseudo_callback, "Mixer Faucet", nullptr);
    if(!mixer_thread)
    {
        puts("Somehow failed to make audio mixing thread. Check your operating environment for corruption. Shutting down mixer faucet.");
        SDL_CloseAudioDevice(device);
        device = 0;
        return false;
    }
    
    SDL_PauseAudioDevice(device, 0);
    
    initiated = true;
    puts("Mixer faucet opened SDL audio device:");
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
            mine->format.volume = volume;
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

DLLEXPORT TYPE_EC fauxmix_emitter_volumes(TYPE_ID id, TYPE_FT left, TYPE_FT right)
{
    float fadewindow = float(got.freq)*0.01f;
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
