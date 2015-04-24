#ifndef INCLUDED_WAVSTREAM
#define INCLUDED_WAVSTREAM

#include "stream.hpp"
#include "emitter.hpp"
#include "format.hpp"

#include <SDL2/SDL_audio.h>

struct wavstream : pcmstream
{
    wavfile sample;
    
    wavstream(const char * filename); // TODO: SEPARATE INSTANTIATION OF WAVSTREAM FROM WAVFILE
    
    Uint32 position; // Position is in OUTPUT SAMPLES, not SOUNDBYTE SAMPLES, i.e. it counts got.freqs not wavstream.freqs
    Uint8 * buffer = nullptr;
    Uint32 bufferlen;
    
    bool ready();
    Uint16 channels();
    void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info);
    void fire(emitterinfo * info);
};
int t_wavfile_load(void * etc);

#endif
