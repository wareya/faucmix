#ifndef INCLUDED_API
#define INCLUDED_API

#define DLLEXPORT extern "C" __attribute__((visibility("default")))

#include "format.hpp"
#include "wavstream.hpp"
#include "stream.hpp"
#include "emitter.hpp"
#include "respondtoSDL.hpp"

#include <SDL2/SDL_audio.h>

#include "global.hpp"

DLLEXPORT void fauxmix_dll_init();

DLLEXPORT bool fauxmix_use_float_output(bool b);

DLLEXPORT bool fauxmix_init(int samplerate, bool mono, int samples);
DLLEXPORT int fauxmix_get_samplerate();
DLLEXPORT int fauxmix_get_channels();
DLLEXPORT int fauxmix_get_samples();
DLLEXPORT bool fauxmix_is_ducking();

/*
 * Samples have to be loaded on a thread or else client game logic would cause synchronous disk IO
 * However, this means that the "load sample" command can't give a definitive response on the validity of a sample file
 * It has to be checked in some other way
 * The idea I just had is:
 * Sample handles can be loaded, loading, unloaded, or failed
 * and you can poll their status
 * and it's up to you to kill them if they fail or unload
 */
#include "wavfile.hpp"
DLLEXPORT wavfile * fauxmix_sample_load(const char * filename);
DLLEXPORT void fauxmix_sample_volume(void * sample, float volume);
DLLEXPORT void fauxmix_sample_kill(void * sample);
DLLEXPORT int fauxmix_sample_status(void * sample);

DLLEXPORT emitter * fauxmix_emitter_create(void * sample);
DLLEXPORT wavfile * fauxmix_emitter_sample(emitter * mine);
DLLEXPORT int fauxmix_emitter_status(emitter * mine);
DLLEXPORT void fauxmix_emitter_fire(emitter * mine);
DLLEXPORT void fauxmix_emitter_cease(emitter * mine);
DLLEXPORT void fauxmix_emitter_kill(emitter * mine);

#endif
