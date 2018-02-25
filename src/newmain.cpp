#include "api.hpp"

#include <thread>
#include <math.h>
#include <string.h>

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
        
    fauxmix_dll_init();

    //fauxmix_use_float_output(true);

    fauxmix_init(48000, 1024);
    
    uint32_t emitter1;
    uint32_t sample1;
    
    if(strcmp("ducker", argv[1]) != 0)
    {
        sample1 = fauxmix_sample_load(argv[1]);
        emitter1 = fauxmix_emitter_create(sample1);
        //fauxmix_emitter_loop(emitter1, true);
        fauxmix_emitter_pitch(emitter1, 1.0f);
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
    
    fauxmix_push();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if(argc <= 2) // normal
    {
        while(fauxmix_emitter_status(emitter1) > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if(fauxmix_sample_status(sample1) == -1)
                break;
            else if(fauxmix_sample_status(sample1) == -2)
                puts("mayday! no such sample in shadow!");
        }
    }
    else // stress test
    {
        while(1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            
            fauxmix_emitter_fire(emitter1);
            if(fauxmix_sample_status(sample1) == -1)
                break;
            else if(fauxmix_sample_status(sample1) == -2)
                puts("mayday! no such sample in shadow!");
        }
    }
    puts("broke out");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}
