#include "api.hpp"

#include <SDL2/SDL.h>

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
        
    fauxmix_dll_init();

    //fauxmix_use_float_output(true);

    fauxmix_init(44100, false, 1024);
    
    Uint32 emitter1;
    
    if(strcmp("ducker", argv[1]) != 0)
    {
        auto sample1 = fauxmix_sample_load(argv[1]);
        emitter1 = fauxmix_emitter_create(sample1);
        fauxmix_emitter_fire(emitter1);
    }
    else
    {
        auto sample1 = fauxmix_sample_load("jibberish.wav");
        auto sample2 = fauxmix_sample_load("8k.wav");
        auto sample3 = fauxmix_sample_load("mote.wav");
        emitter1 = fauxmix_emitter_create(sample1);
        auto emitter2 = fauxmix_emitter_create(sample2);
        auto emitter3 = fauxmix_emitter_create(sample3);
        fauxmix_emitter_fire(emitter1);
        fauxmix_emitter_fire(emitter2);
        fauxmix_emitter_fire(emitter3);
    }
    
    SDL_Delay(1000);
    while(fauxmix_emitter_status(emitter1))
    {
        SDL_Delay(1000);
    }
    
    return 0;
}
