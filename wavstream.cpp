#include "wavstream.hpp"
#include "resample.hpp"
#include "format.hpp"
#include "lcm.hpp"

wavstream::wavstream(wavfile * given) // TODO: SEPARATE INSTANTIATION OF WAVSTREAM FROM WAVFILE
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
    if(sample->status <= 0)
    {
        puts("not ready");
        return nullptr;
    }
    if(!sample->data)
    {
        puts("no data");
        return nullptr;
    }
    
    Uint32 specblock = spec->channels*(SDL_AUDIO_BITSIZE(spec->format))/8; // spec blocksize
    if(specblock == 0)
    {
        puts("invalid spec");
        return nullptr;
    }
    
    auto outputsize = len; // output bytes 
    len /= specblock; // output samples
    if(outputsize != bufferlen and buffer != nullptr)
    {
        free(buffer);
        buffer = nullptr;
    }
    if(buffer == nullptr)
    {
        buffer = (Uint8 *)malloc(outputsize);
        bufferlen = outputsize;
    }
    
    auto fuck = audiospec_to_wavformat(spec);
    auto fuck2 = sample->format;
    fuck2.samplerate *= info->ratefactor;
    linear_resample_into_buffer
    ( position
    , &fuck2
    , sample->data, sample->bytes
    , buffer, bufferlen
    , &fuck, info->loop);
    
    position += len;
    
    if(info->loop)
    {
        position = position % (sample->samples*spec->freq);
    }
    else if(position*(Uint64)sample->format.samplerate/spec->freq > sample->samples)
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
