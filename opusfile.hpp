#ifndef INCLUDED_WAVFLIE
#define INCLUDED_WAVFLIE

#include "format.hpp"
#include "wavfile.hpp"

#include <atomic>
#include <string>

wavfile * opusfile_load (const char * filename);

int t_opusfile_load(void * etc);

#endif
