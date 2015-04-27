// Coypright 2015 Alexander "wareya" Nadeau <wareya@gmail.com>
// Available under either the ISC or zlib license.

#include "emitter.hpp"
#include "wavstream.hpp"

#include <vector>
#include <deque>

#include <SDL2/SDL.h>
#undef main
#include <stdio.h>
#include <iostream>

void respondtoSDL(void * udata, Uint8 * stream, int len)
{
    auto& spec = *(SDL_AudioSpec *)udata;
    
    for(auto stream : emitters)
    {
        auto f = stream->generateframe(&spec, len);
        if(f != nullptr)
        {
            responses.push_back(f);
        }
    }
    //while(used < len)
    {
        if(responses.size()>0)
            memcpy(stream, responses[0], len);
        
        // TODO: mix responses
        //len++;
    }
    responses.clear();
    fwrite(stream, 1, len, dumpfile);
    fflush(dumpfile);
}

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
    wavstream sample(argv[1]);
    
    dumpfile = fopen("here!.raw", "wb");
    
    emitter output;
    output.stream = &sample;
    output.info.pan = 0.0f;
    output.info.volume = 1.0f;
    output.info.playing = true;
    output.info.loop = true;
    output.info.mixdown = 1.0f;
    
    emitters.push_back(&output);
    
    
    output.fire();
    
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
