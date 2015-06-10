#ifndef INCLUDED_RESPOND
#define INCLUDED_RESPOND

#include "emitter.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <vector>
#include <set>

void respondtoSDL(void * udata, Uint8 * stream, int len);

#endif
