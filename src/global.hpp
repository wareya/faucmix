#ifndef INCLUDED_GLOBAL
#define INCLUDED_GLOBAL

/*
* 4 types of API calls:
* Setup
* Hybrid read-write
* Write
* Read
* 
* "Setup" commands configure the internal state of the library.
* 
* "Hybrid" commands mutate data, but need to return with a meaningful "handle".
*  The handle is managed by the game thread. The data is managed by the engine.
* 
* "Write" commands mutate data, but to do so they queue up the command metadata
*  for the engine thread to deal with.
* 
* "Read" commands return delayed, mirrored data.
* 
* init:
* -cfg
* 
* op-hybrid:
* -handles rw
* -buffer write (MUTEX)
* 
* op-write:
* -handles read
* -buffer write (MUTEX)
* 
* op-read:
* -handles read
* -shadow read (MUTEX)
*/

#include <functional>
#include <vector>
#include <deque>
#include <mutex>
#include <map>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_audio.h>

/* cfg */

extern bool initiated;
extern float volume;
extern float ducker;

extern SDL_AudioSpec want;
extern SDL_AudioSpec got;

/* cmdbuffer */

struct command
{
    std::function<void()> func;
};

extern std::mutex commandlock;
extern std::deque<command> cmdbuffer;
extern std::deque<command> copybuffer;

/* shadow */

struct emitterdat
{
    int status;
    float vol1;
    float vol2;
};

struct sampledat
{
    int status;
    float vol;
};

extern std::mutex shadowlock;
extern std::map<Uint32, emitterdat> emittershadow;
extern std::map<Uint32, sampledat> sampleshadow;

/* lists */ 
#include "genericallocator.hpp"

#include "emitter.hpp"
/* for game thread */
extern GenericAllocator<Uint32> emitterids;
/* for audio thread */
extern std::map<Uint32, emitter *> emitters;

#include "wavfile.hpp"
/* for game thread */
extern GenericAllocator<Uint32> sampleids;
/* for audio thread */
extern std::map<Uint32, wavfile *> samples;

// used to kill wav-emitters when a wav-sample is killed
extern std::map<Uint32, std::vector<Uint32>> samplestoemitters;
extern std::map<Uint32, float> mixchannels; // unimplemented

#endif
