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
    
    /* Do commands */
    std::deque<command> copybuffer;
    commandlock.lock();
        while(cmdbuffer.size() > 0)
        {
            copybuffer.push_back(cmdbuffer[0]);
            cmdbuffer.pop_front();
        }
        
        // the following must be inside of the commandlock for thread safety purposes
        double thisframe = get_us();
    commandlock.unlock();
    
    // this is outside of the commandlock because it can cause dynamic reallocations, but it's logically connected to the get_us line.
    
        // we need to store the time that we strt mixing so that we can compare it 
        /* THE DRIVER THREAD DOES NOT NECESSARILY HAVE RELIABLE FRAME TIMING. WE NEED TO FILTER SEVERAL FRAME TIMINGS TOGETHER IN ORDER TO SYNCHRONIZE WITH MESSAGES. */
        while(timestamps.size() > 3)
            timestamps.pop_back();
        
        const double frametime = double(len/block)/fmt.samplerate*1000000;
        
        timestamps.push_front(thisframe);
        double start = 0;
        
        float divisor = 1;
        for(auto i = 0; i < timestamps.size(); i++)
        {
            start += timestamps[i];
            start += i*frametime;
        }
        start /= timestamps.size();
        //std::cout << thisframe << " vs " << start << "\n";
    
    // all commands up to this point should (deterministically speaking) have times from before [start]. So we add a delay to their timestamp which is as long as their expected backwards latency.
    int cmddelay = len/block;
    
    /* generate samples in between executing commands */
    
    int used = 0; // number of samples generated
    for(int i = 0; i <= copybuffer.size()/*yes -- one more stream iteration than number of commands*/; i++)
    {
        int subwindow; // length in stream up to next command or end of stream
        if(i < copybuffer.size())
        {
            if(copybuffer[i].us > start) // this might happen if the clock changes while running, can never be too safe
            {
                copybuffer[i].us = start;
                puts("fixed clock!");
            }
            double offset = copybuffer[i].us/1000000*fmt.samplerate - start/1000000*fmt.samplerate;
            subwindow = offset + cmddelay - used;
            subwindow *= block;
            if(used*block + subwindow > len)
            {
                subwindow = len - used*block;
                puts("fixed clock again!");
            }
        }
        else
            subwindow = len - used*block;
        
        /* Get emitter responses */
        if(subwindow > 0)
        {
            for(auto s : emitters)
            {
                auto stream = s.second;
                auto f = stream->generateframe(&spec, subwindow);
                if(f != nullptr)
                {
                    responses.push_back(f);
                    infos.push_back(&stream->mix);
                }
            }
            /* Mix responses into output stream */
            long int maxloops = len;
            int prog = 0;
            while(used*block < len and prog*block < subwindow and maxloops > 0)
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
                        
                        transient[c] += get_sample((Uint8*)response+(prog*channels+c)*sample, &fmt) * realvol;
                        
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
                {
                    set_sample(stream+(used*channels+c)*sample, &fmt, transient[c]/ducker);
                }
                used += 1;
                prog += 1;
            }
            responses.clear();
            infos.clear();
        }
        
        if(i < copybuffer.size()) // run command from this iteration
            copybuffer[i].func();
    }
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