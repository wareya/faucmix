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

std::set<emitter *> emitters;
std::vector<void *> responses;

void respondtoSDL(void * udata, Uint8 * stream, int len)
{
    auto& spec = *(SDL_AudioSpec *)udata;
    auto fmt = audiospec_to_wavformat(&spec);
    
    auto block = fmt.blocksize;
    auto sample = fmt.bytespersample;
    int channels = spec.channels;
    
    std::cout << block << " " << int(sample) << " " << channels << "\n";
    
    for(auto stream : emitters)
    {
        auto f = stream->generateframe(&spec, len);
        if(f != nullptr)
        {
            responses.push_back(f);
        }
        else
        {
            puts("Empty callback");
        }
    }
    int used = 0;
    while(used*block < len)
    {
        ducker -= 20.0/fmt.samplerate;
        if(ducker < 1.0f)
            ducker = 1.0f;
        
        std::vector<float> transient(channels);
        for(auto c = 0; c < channels; c++)
        {
            transient[c] = 0.0f;
            for(auto response : responses)
                transient[c] += get_sample((Uint8*)response+(used*channels+c)*sample, &fmt);
            
            if(fabsf(transient[c]) > ducker)
                ducker = fabsf(transient[c]);
        }
        for(auto c = 0; c < spec.channels; c++)
        {
            set_sample(stream+(used*channels+c)*sample, &fmt, transient[c]);
        }
        used += 1;
    }
    responses.clear();
}
