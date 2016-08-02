// Coypright 2015 Alexander "wareya" Nadeau <wareya@gmail.com>
// Available under either the ISC or zlib license.

#include "respondtoSDL.hpp"

#include "emitter.hpp"
#include "wavstream.hpp"
#include "global.hpp"

#include "btime.hpp"

#include <vector>
#include <deque>
#include <set>

#include <stdio.h>
#include <iostream>
#include <math.h>

#include <stdlib.h>

std::vector<void *> responses;
std::vector<mixinfo *> infos;

std::deque<double> timestamps;

void respondtoSDL(void * udata, Uint8 * stream, int len)
{
    // get the output stream spec in a way that certain faucet mixer functions understand
    auto& spec = *(SDL_AudioSpec *)udata;
    auto fmt = audiospec_to_wavformat(&spec);
    
    auto block = fmt.blocksize;
    auto sample = fmt.bytespersample;
    int channels = spec.channels;
    
    /* Get emitter responses */
    for(auto s : emitters)
    {
        auto stream = s.second;
        auto f = stream->generateframe(&spec, len);
        if(f != nullptr)
        {
            responses.push_back(f);
            infos.push_back(&stream->mix);
        }
    }
    /* Mix responses into output stream */
    // If you can't read this loop without comments, *please don't change it.*
    // It's left uncommented on purpose to keep people who can't read it away.
    long int maxloops = len;
    long int used = 0;
    while(used*block < len and maxloops > 0)
    {
        maxloops -= 1;
        if(ducker > 1.0f)
        {
            ducker -= 1.0f/fmt.samplerate;
            if(ducker < 1.0f)
                ducker = 1.0f;
        }
        if(ducker < 1.0f) // failsafe
            ducker = 1.0f;
        
        std::vector<float> transient(channels);
        for(auto c = 0; c < channels; c++)
        {
            transient[c] = 0.0f;
            for(auto i = 0; i < responses.size(); i++)
            {
                auto response = responses[i];
                auto& info = infos[i];
                
                float realvol;
                if (channels == 2)
                    realvol = ((c == 0)?info->vol_l:info->vol_r);
                else
                    realvol = (info->vol_l + info->vol_r)/2;
                if(mixchannels.count(info->channel))
                    realvol *= mixchannels[info->channel];
                
                transient[c] += get_sample((Uint8*)response+(used*channels+c)*sample, &fmt) * realvol;
                
                if(c+1 == channels and info->remaining > 0)
                {
                    info->vol_l += info->add_l;
                    info->vol_r += info->add_r;
                    info->remaining -= 1;
                    if(info->remaining == 0)
                    {
                        info->vol_l = info->target_l;
                        info->vol_r = info->target_r;
                    }
                }
            }
            if(fabsf(transient[c]) > ducker)
            {
                ducker = fabsf(transient[c])*1.1f; // making the brickwall ducker overduck results in higher ducker quality???? WTF
                puts("Ducking!");
            }
        }
        for(auto c = 0; c < spec.channels; c++)
            set_sample(stream+(used*channels+c)*sample, &fmt, transient[c]/ducker);
        
        used += 1;
    }
}

void non_time_critical_update_cleanup()
{
    responses.clear();
    infos.clear();
    
    /* Build shadow data */
    emittershadow.clear();
    sampleshadow.clear();
    for(auto s : emitters)
    {
        auto id = s.first;
        auto mine = s.second;
        emittershadow[id].status = mine->info.playing;
        emittershadow[id].vol1 = mine->mix.vol_l;
        emittershadow[id].vol2 = mine->mix.vol_r;
    }
    for(auto s : samples)
    {
        auto id = s.first;
        auto sample = s.second;
        sampleshadow[id].status = sample->status; // sample status (reflects loading state) can be written by a background thread but is atomic
        sampleshadow[id].vol = sample->format.volume;
    }
}
