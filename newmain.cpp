#include "api.hpp"

#include <SDL2/SDL.h>

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
        
    fauxmix_dll_init();

    //fauxmix_use_float_output(true);

    fauxmix_init(44100, false, 1024);
    
    auto sample1 = fauxmix_sample_load(argv[1]);
    auto emitter1 = fauxmix_emitter_create(sample1);
    fauxmix_emitter_fire(emitter1);
    
    /*
    auto sample1 = fauxmix_sample_load("8k.wav");
    auto sample2 = fauxmix_sample_load("sin.wav");
    auto sample3 = fauxmix_sample_load("clickfloat.wav");
    auto sample4 = fauxmix_sample_load("jibberish.wav");
    auto sample5 = fauxmix_sample_load("mote.wav");
    auto sample6 = fauxmix_sample_load("dc.wav");
    auto emitter1 = fauxmix_emitter_create(sample1);
    fauxmix_emitter_fire(emitter1);
    auto emitter2 = fauxmix_emitter_create(sample2);
    fauxmix_emitter_fire(emitter2);
    auto emitter3 = fauxmix_emitter_create(sample3);
    fauxmix_emitter_fire(emitter3);
    auto emitter4 = fauxmix_emitter_create(sample4);
    fauxmix_emitter_fire(emitter4);
    auto emitter5 = fauxmix_emitter_create(sample5);
    fauxmix_emitter_fire(emitter5);
    auto emitter6 = fauxmix_emitter_create(sample6);
    fauxmix_emitter_fire(emitter6);*/
    
    while(emitter1->info.playing)
    {
        SDL_Delay(1000);
        printf("%i\n", int(sample1->status));
    }
    
    return 0;
}
