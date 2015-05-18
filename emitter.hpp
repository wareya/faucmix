#ifndef INCLUDED_EMITTER
#define INCLUDED_EMITTER

#include "stream.hpp"
#include "format.hpp"
#include "wavfile.hpp"

#include <SDL2/SDL_audio.h>

struct emitter
{
    pcmstream * stream;
    emitterinfo info;
    
    emitter(wavfile * sample);
    ~emitter();
    
    void * DSPbuffer = nullptr;
    size_t DSPlen;
    
    void * generateframe(SDL_AudioSpec * spec, unsigned int len);
    void fire();
    void cease();
};

#endif
