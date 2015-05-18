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

std::set<emitter *> emitters;
std::vector<void *> responses;

void respondtoSDL(void * udata, Uint8 * stream, int len)
{
    auto& spec = *(SDL_AudioSpec *)udata;
    auto fmt = audiospec_to_wavformat(&spec);
    
    auto sample = fmt.bytespersample;
    auto block = fmt.blocksize;
    const int channels = spec.channels;
    
    for(auto stream : emitters)
    {
        auto f = stream->generateframe(&spec, len);
        if(f != nullptr)
        {
            responses.push_back(f);
        }
    }
    int used = 0;
    while(used < len)
    {
        ducker -= 20.0/fmt.samplerate;
        if(ducker < 1.0f)
            ducker = 1.0f;
        
        std::vector<float> transient(channels);
        for(auto i = 0; i < spec.channels; i++)
        {
            //transient[i] = 0.0f;
            for(auto response : responses)
                transient[i] += get_sample((char*)(response)+(used*block)+(i*sample), &fmt);
            
            if(fabsf(transient[i]) > ducker)
                ducker = fabsf(transient[i]);
        }
        for(auto i = 0; i < spec.channels; i++)
        {
            set_sample((char*)(stream)+(used*block)+(i*sample), &fmt, transient[i]/ducker);
        }
        used++;
    }
    responses.clear();
    //fwrite(stream, 1, len, dumpfile);
    //fflush(dumpfile);
}
