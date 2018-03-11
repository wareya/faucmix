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
    if(sample->buffer.ultimate_length == 0 or sample->buffer.array == nullptr)
    {
        puts("no buffer");
        return nullptr;
    }
    
    // reallocate scratch buffer if we need to generate more data than it can hold
    if(resample_buffer.array == nullptr or count*channels > resample_buffer.ultimate_length)
        resample_buffer.resize(count, channels);
    
    // downmix (note: any channel number mismatch makes the soundbyte be downsample to mono, then up to the output channel count)
    float * from_buffer;
    if(channels == sample->buffer.channels)
        from_buffer = sample->buffer.array;
    else
    {
        if(downmix_buffer.array == nullptr or downmix_buffer.ultimate_length != sample->buffer.samples*channels)
            downmix_buffer.resize(sample->buffer.samples, channels);
        for(uint64_t i = 0; i < sample->buffer.samples; i++)
        {
            float sum = 0;
            for(uint64_t c = 0; c < sample->buffer.channels; c++)
                sum += sample->buffer[i*sample->buffer.channels + c];
            sum /= sample->buffer.channels;
            for(uint64_t c = 0; c < channels; c++)
                downmix_buffer[i*channels + c] = sum;
        }
        from_buffer = downmix_buffer.array;
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
    linear_resample_into_buffer
    ( position
    , channels
    , from_buffer
    , sample->buffer.samples
    , sample->samplerate
    , resample_buffer.array
    , resample_buffer.samples
    , samplerate
    , info->loop);
    
    // increase the playback progress point based on how many output samples we consumed
    position += count;
    
    // if our sample is looping we wrap around the position using positive modulus
    if(info->loop)
    {
        position = position % (sample->buffer.samples*samplerate);
    }
    // otherwise we stop playing when the position passes the length of the sample (we do some stupid math to put them on the same samplerate space)
    else if(position*(uint64_t)sample->samplerate > sample->buffer.samples*samplerate)
    {
        info->playing = false;
        info->complete = true;
    }
    
    return resample_buffer.array;
}
void wavstream::fire(emitterinfo * info)
{
    info->complete = false;
    position = 0;
    info->playing = true;
}
void wavstream::cease(emitterinfo * info)
{
    position = 0;
    info->playing = false;
}
