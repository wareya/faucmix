#ifndef INCLUDED_CHANNELS
#define INCLUDED_CHANNELS

#include <SDL2/SDL_audio.h>

// writes n width-byte-wide values to tgt
int memwrite(void * tgt, Uint64 value, size_t n, int width);

// srcbuffer must be smaller than tgtbuffer in *samples*
int channel_cvt(void * tgtbuffer, void * srcbuffer, Uint32 samples, SDL_AudioSpec * tgtspec, Uint16 srcchannels);

#endif
