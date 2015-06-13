// Coypright 2015 Alexander "wareya" Nadeau <wareya@gmail.com>
// Available under either the ISC or zlib license.

#include "respondtoSDL.hpp"

#include "emitter.hpp"
#include "wavstream.hpp"
#include "global.hpp"

#include <vector>
#include <deque>
#include <set>

#include <stdio.h>
#include <iostream>
#include <math.h>

#include <stdlib.h>

std::vector<void *> responses;
std::vector<mixinfo *> infos;

void respondtoSDL(void * udata, Uint8 * stream, int len)
{
    auto& spec = *(SDL_AudioSpec *)udata;
    auto fmt = audiospec_to_wavformat(&spec);
    
    auto block = fmt.blocksize;
    auto sample = fmt.bytespersample;
    int channels = spec.channels;
    
    /* Do commands */
    std::deque<command> copybuffer;
    commandlock.lock();
        while(cmdbuffer.size() > 0)
        {
            copybuffer.push_back(cmdbuffer[0]);
            cmdbuffer.pop_front();
        }
    commandlock.unlock();
    
    for(auto command : copybuffer)
    {
        command();
    }
    
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
    int used = 0;
    while(used*block < len)
    {
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
                if (channels == 2)
                    transient[c] += get_sample((Uint8*)response+(used*channels+c)*sample, &fmt) * ((c == 0)?info->vol_l:info->vol_r);
                else
                    transient[c] += get_sample((Uint8*)response+(used*channels+c)*sample, &fmt) * (info->vol_l + info->vol_r)/2;
                
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
                ducker = fabsf(transient[c])*1.1f; // making the brickwall ducker overduck results in higher ducker quality???? WTF
        }
        for(auto c = 0; c < spec.channels; c++)
        {
            set_sample(stream+(used*channels+c)*sample, &fmt, transient[c]/ducker);
        }
        used += 1;
    }
    responses.clear();
    
    /* Build shadow data */
    shadowlock.lock();
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
            sampleshadow[id].status = sample->status;
            sampleshadow[id].vol = sample->volume;
        }
    shadowlock.unlock();
}
