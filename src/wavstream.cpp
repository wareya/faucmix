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
Uint16 wavstream::channels()
{
    return sample->format.channels;
}
void * wavstream::generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info)
{
    // If the sample hasn't loaded yet we can't use it
    if(sample->status <= 0)
    {
        puts("not ready");
        return nullptr;
    }
    // If the sample has no data for some reason we can't use it
    if(!sample->data)
    {
        puts("no data");
        return nullptr;
    }
    
    
    Uint32 specblock = spec->channels*(SDL_AUDIO_BITSIZE(spec->format))/8; // spec blocksize
    if(specblock == 0)
    {   // make sure the audio spec of the sample is sensible
        puts("invalid spec");
        return nullptr;
    }
    
    auto outputsize = len; // output bytes 
    len /= specblock; // output samples
    // reallocate scratch buffer if we need to generate more data than it can hold
    if(outputsize > bufferlen and buffer != nullptr)
    {
        std::cout << "reallocating buffer " << bufferlen << " to " << outputsize << "\n";
        free(buffer);
        buffer = nullptr;
    }
    // I don't actually know how realloc works so I free and malloc separately manually
    // We don't need the data that was previously in the scratch buffer anyways
    if(buffer == nullptr)
    {
        buffer = (Uint8 *)malloc(outputsize);
        bufferlen = outputsize;
    }
    
    // resample the sample data into the scratch buffer starting at the playback progress point
    // we track the progress using output samples, *NOT* storage samples, in order to use all integer math
    auto fuck = audiospec_to_wavformat(spec);
    auto fuck2 = sample->format;
    fuck2.samplerate *= info->ratefactor;
    linear_resample_into_buffer
    ( position
    , &fuck2
    , sample->data, sample->bytes
    , buffer, bufferlen
    , &fuck, info->loop);
    
    // increase the playback progress point based on how many output samples we consumed
    position += len;
    
    // if our sample is looping we wrap around the position using positive modulus
    if(info->loop)
    {
        position = position % (sample->samples*spec->freq);
    }
    // otherwise we stop playing when the position passes the length of the sample (we do some stupid math to put them on the same samplerate space)
    // FIXME: Should the integer cast of the sample rate conversion of the audio file sample count floor of ceil? integer division floors by default
    else if(position*(Uint64)fuck2.samplerate/spec->freq > sample->samples)
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
