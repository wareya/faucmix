#ifndef NO_OPUS

#ifndef INCLUDED_OPUSFLIE
#define INCLUDED_OPUSFLIE

#include "format.hpp"
#include "wavfile.hpp"

#include <atomic>
#include <string>

wavfile * opusfile_load (const char * filename);

#endif

#endif
