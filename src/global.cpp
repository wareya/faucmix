#include "global.hpp"

/* cmdbuffer */

std::mutex commandlock;
std::deque<command> cmdbuffer;
std::deque<command> copybuffer;

/* shadow */

std::mutex shadowlock;
std::map<Uint32, emitterdat> emittershadow;
std::map<Uint32, sampledat> sampleshadow;

/* cfg */

bool initiated;
float volume;
float ducker;

/* lists */

GenericAllocator<Uint32> emitterids;
std::map<Uint32, emitter *> emitters;
GenericAllocator<Uint32> sampleids;
std::map<Uint32, wavfile *> samples;
std::map<Uint32, std::vector<Uint32>> samplestoemitters;
std::map<Uint32, float> mixchannels; // unimplemented
