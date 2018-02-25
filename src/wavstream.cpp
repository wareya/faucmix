#include "wavstream.hpp"
#include "resample.hpp"
#include "format.hpp"
#include "lcm.hpp"

#include <iostream>

wavstream::wavstream(wavfile * given)
{
    sample = given;
}
wavstream::~wavstream()
{}
bool wavstream::ready()
{
    return sample->status > 0;
}
float * wavstream::generateframe(uint64_t count, uint64_t channels, uint64_t samplerate, emitterinfo * info)
{
    // If the sample hasn't loaded yet we can't use it
    if(sample->status <= 0)
    {
        puts("not ready");
        return nullptr;
    }
    // If the sample has no data for some reason we can't use it
    if(!sample->buffer)
    {
        puts("no buffer");
        return nullptr;
    }
    
    // reallocate scratch buffer if we need to generate more data than it can hold
    if(count > bufferlen and buffer != nullptr)
    {
        free(buffer);
        buffer = nullptr;
    }
    if(buffer == nullptr)
    {
        bufferlen = count;
        buffer = (float*)malloc(bufferlen*channels*sizeof(float));
    }
/*
( uint64_t position
, uint64_t channels
, void * source
, uint64_t sourcelen
, uint64_t sourcefreq
, void * target
, uint64_t targetlen
, uint64_t targetfreq
, bool looparound
*/
    // resample the sample data into the scratch buffer starting at the playback progress point
    // we track the progress using output samples, *NOT* storage samples, in order to use all integer math
    // FIXME: need to convert between mono/stereo somewhere between here and the mixer
    linear_resample_into_buffer
    ( position
    , channels
    , sample->buffer
    , sample->samples
    , sample->samplerate
    , buffer
    , bufferlen
    , samplerate
    , info->loop);
    
    // increase the playback progress point based on how many output samples we consumed
    position += count;
    
    // if our sample is looping we wrap around the position using positive modulus
    if(info->loop)
    {
        position = position % (sample->samples*samplerate);
    }
    // otherwise we stop playing when the position passes the length of the sample (we do some stupid math to put them on the same samplerate space)
    // FIXME: Should the integer cast of the sample rate conversion of the audio file sample count floor of ceil? integer division floors by default
    else if(position*(uint64_t)sample->samplerate/samplerate > sample->samples)
    {
        info->playing = false;
    }
    
    return buffer;
}
void wavstream::fire(emitterinfo * info)
{
    position = 0;
    info->playing = true;
}
void wavstream::cease(emitterinfo * info)
{
    position = 0;
    info->playing = false;
}
