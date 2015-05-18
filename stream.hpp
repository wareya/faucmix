#ifndef INCLUDED_STREAM
#define INCLUDED_STREAM

#include "format.hpp"

#include <SDL2/SDL_types.h>
#include <SDL2/SDL_audio.h>

struct pcmstream
{
    int samplerate;
    virtual void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info);
    virtual bool isplaying();
    virtual bool ready();
    virtual Uint16 channels();
    virtual void fire(emitterinfo * info);
    virtual void cease(emitterinfo * info);
    
    virtual ~pcmstream() {};
};

#endif
