#include "global.hpp"

/* cmdbuffer */

std::mutex commandlock;
std::deque<command> cmdbuffer;
std::deque<command> copybuffer;

/* shadow */

std::mutex shadowlock;
std::map<uint32_t, emitterdat> emittershadow;
std::map<uint32_t, sampledat> sampleshadow;

/* cfg */

bool initiated;
float volume;
float ducker;

/* lists */

GenericAllocator<uint32_t> emitterids;
std::map<uint32_t, emitter *> emitters;
GenericAllocator<uint32_t> sampleids;
std::map<uint32_t, wavfile *> samples;
std::map<uint32_t, std::vector<uint32_t>> samplestoemitters;
std::map<uint32_t, float> mixchannels; // unimplemented
