#ifndef INCLUDED_EMITTER
#define INCLUDED_EMITTER

#include "stream.hpp"
#include "format.hpp"

#include <SDL2/SDL_audio.h>

struct emitter
{
    pcmstream * stream;
    emitterinfo info;
    
    void * DSPbuffer = nullptr;
    size_t DSPlen;
    
    void * generateframe(SDL_AudioSpec * spec, unsigned int len);
    void fire();
};

#endif
