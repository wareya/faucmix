#ifndef INCLUDED_WAVSTREAM
#define INCLUDED_WAVSTREAM

#include "stream.hpp"
#include "emitter.hpp"
#include "format.hpp"
#include "wavfile.hpp"

struct wavstream : pcmstream
{
    wavfile * sample;
    
    wavstream(wavfile * given);
    
    uint64_t position; // Position is in OUTPUT SAMPLES, not SOUNDBYTE SAMPLES
    float * buffer = nullptr;
    uint64_t bufferlen;
    
    bool ready();
    float * generateframe(uint64_t count, uint64_t channels, uint64_t samplerate, emitterinfo * info);
    void fire(emitterinfo * info);
    void cease(emitterinfo * info);
    
    ~wavstream();
};

#endif
