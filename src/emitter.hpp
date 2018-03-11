#ifndef INCLUDED_EMITTER
#define INCLUDED_EMITTER

#include "stream.hpp"
#include "format.hpp"
#include "wavfile.hpp"

// Mixinfo is handled by the mixer, not the emitter
struct mixinfo
{
    float vol_l = 1.0f;
    float vol_r = 1.0f;
    
    float add_l = 0.0f;
    float add_r = 0.0f;
    
    float target_l = 1.0f;
    float target_r = 1.0f;
    
    int remaining = 0;
    
    int channel = -1;
};

struct emitter
{
    pcmstream * stream;
    emitterinfo info;
    
    mixinfo mix;
    
    bool transient = false;
    
    emitter(wavfile * sample, bool transient = false);
    ~emitter();
    
    void * DSPbuffer = nullptr;
    size_t DSPlen;
    
    float * generateframe(uint64_t count, uint64_t channels, uint64_t samplerate);
    void fire();
    void cease();
};

#endif
