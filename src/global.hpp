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

// cfg

extern bool initiated;
extern float volume;
extern float ducker;

// cmdbuffer

struct command
{
    std::function<void()> func;
};

extern std::mutex commandlock;
extern std::deque<command> cmdbuffer;
extern std::deque<command> copybuffer;

// shadow

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
extern std::map<uint32_t, emitterdat> emittershadow;
extern std::map<uint32_t, sampledat> sampleshadow;

/* lists */ 
#include "genericallocator.hpp"

#include "emitter.hpp"
/* for game thread */
extern GenericAllocator<uint32_t> emitterids;
/* for audio thread */
extern std::map<uint32_t, emitter *> emitters;

#include "wavfile.hpp"
/* for game thread */
extern GenericAllocator<uint32_t> sampleids;
/* for audio thread */
extern std::map<uint32_t, wavfile *> samples;

// used to kill wav-emitters when a wav-sample is killed
extern std::map<uint32_t, std::vector<uint32_t>> samplestoemitters;
extern std::map<uint32_t, float> mixchannels; // unimplemented

#endif
