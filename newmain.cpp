#include "api.hpp"

#include <SDL2/SDL.h>

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
        
    fauxmix_dll_init();

    //fauxmix_use_float_output(true);

    fauxmix_init(44100, false, 1024);
    
    auto sample = fauxmix_sample_load(argv[1]);
    auto emitter = fauxmix_emitter_create(sample);
    fauxmix_emitter_fire(emitter);
    
    while(emitter->info.playing)
    {
        printf("%i\n", int(sample->status));
    }
    
    return 0;
}
