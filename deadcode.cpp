// Coypright 2015 Alexander "wareya" Nadeau <wareya@gmail.com>
// Available under either the ISC or zlib license.

#include "emitter.hpp"
#include "wavstream.hpp"
#include "global.hpp"

#include <vector>
#include <deque>

#include <SDL2/SDL.h>
#undef main
#include <stdio.h>
#include <iostream>

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
        for(i = 0; i < spec.channels; i++)
        {
            //transient[i] = 0.0f;
            for(response : responses)
                transient[i] += get_sample((char*)(response)+(used*block)+(i*sample), &fmt);
            
            if(fabsf(transient[i]) > ducker)
                ducker = fabsf(transient[i];
        }
        for(i = 0; i < spec.channels; i++)
        {
            set_sample((char*)(stream)+(used*block)+(i*sample), &fmt, transient[i]/ducker);
        }
        used++;
    }
    responses.clear();
    fwrite(stream, 1, len, dumpfile);
    fflush(dumpfile);
}

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
        
    sample = fauxmix_sample_load(argv[1]);
    wavstream sample(argv[1]);
    
    dumpfile = fopen("here!.raw", "wb");
    
    SDL_PauseAudio(0);
    
    while(output.info.playing)
    {
        SDL_Delay(1000);
        emitter output2;
        output2.stream = &sample;
        output2.info.pan = 0.0f;
        output2.info.volume = 1.0f;
        output2.info.playing = true;
        output2.info.loop = true;
        output2.info.mixdown = 1.0f;
    }
    
    return 0;
}
