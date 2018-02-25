#ifndef INCLUDED_STREAM
#define INCLUDED_STREAM

#include "format.hpp"

struct pcmstream
{
    int samplerate;
    virtual float * generateframe(uint64_t count, uint64_t channels, uint64_t samplerate, emitterinfo * info);
    virtual bool isplaying();
    virtual bool ready();
    virtual void fire(emitterinfo * info);
    virtual void cease(emitterinfo * info);
    
    virtual ~pcmstream() {};
};

#endif
