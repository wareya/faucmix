#ifndef INCLUDED_RESPOND
#define INCLUDED_RESPOND

#include "emitter.hpp"

void mix(float * buffer, uint64_t count, uint64_t channels, uint64_t samplerate);
void non_time_critical_update_cleanup();

#endif
