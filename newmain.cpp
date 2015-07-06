#include "api.hpp"

#include <SDL2/SDL.h>
#undef main

#include <math.h>

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
        
    fauxmix_dll_init();

    //fauxmix_use_float_output(true);

    fauxmix_init(44100, false, 1024);
    
    Uint32 emitter1;
    Uint32 sample1;
    
    if(strcmp("ducker", argv[1]) != 0)
    {
        sample1 = fauxmix_sample_load(argv[1]);
        emitter1 = fauxmix_emitter_create(sample1);
        //fauxmix_emitter_loop(emitter1, true);
        fauxmix_emitter_pitch(emitter1, 2.0f);
        fauxmix_emitter_fire(emitter1);
    }
    else
    {
        sample1 = fauxmix_sample_load("jibberish.wav");
        auto sample2 = fauxmix_sample_load("8k.wav");
        auto sample3 = fauxmix_sample_load("mote.wav");
        emitter1 = fauxmix_emitter_create(sample1);
        auto emitter2 = fauxmix_emitter_create(sample2);
        auto emitter3 = fauxmix_emitter_create(sample3);
        fauxmix_emitter_fire(emitter1);
        fauxmix_emitter_fire(emitter2);
        fauxmix_emitter_fire(emitter3);
    }
    
    SDL_Delay(50);
    while(fauxmix_emitter_status(emitter1))
    {
        SDL_Delay(100);
        if(fauxmix_sample_status(sample1) == -1)
            break;
    }
    
    return 0;
}
