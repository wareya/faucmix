#ifndef INCLUDED_RESAMPLE
#define INCLUDED_RESAMPLE

#include "format.hpp"

// Takes N channels, gives N channels
int linear_resample_into_buffer
( uint64_t position
, uint64_t channels
, float * source
, uint64_t sourcelen
, uint64_t sourcefreq
, float * target
, uint64_t targetlen
, uint64_t targetfreq
, bool looparound
);

#endif
