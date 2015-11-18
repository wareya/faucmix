#ifndef INCLUDED_WAVSTREAM
#define INCLUDED_WAVSTREAM

#include "stream.hpp"
#include "emitter.hpp"
#include "format.hpp"
#include "wavfile.hpp"

#include <SDL2/SDL_audio.h>

struct wavstream : pcmstream
{
    wavfile * sample;
    
    wavstream(wavfile * given);
    
    Uint32 position; // Position is in OUTPUT SAMPLES, not SOUNDBYTE SAMPLES, i.e. it counts got.freqs not wavstream.freqs
    Uint8 * buffer = nullptr;
    Uint32 bufferlen;
    
    bool ready();
    Uint16 channels();
    void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info);
    void fire(emitterinfo * info);
    void cease(emitterinfo * info);
    
    ~wavstream();

};
int t_wavfile_load(void * etc);

#endif
