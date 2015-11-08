#ifndef INCLUDED_RESAMPLE
#define INCLUDED_RESAMPLE

#include "format.hpp"

#include <SDL2/SDL_types.h>

// Takes N channels, gives N channels
int linear_resample_into_buffer
( Uint32 position
, wavformat * srcfmt
, void * src
, Uint32 srclen
, void * tgt
, Uint32 tgtlen
, wavformat * tgtfmt
, bool looparound
);

#endif
