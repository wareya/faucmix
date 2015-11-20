#include "format.hpp"

#include <SDL2/SDL_audio.h>

// We don't use SDL native formats internally. We have our own format that's like a simplified .wav spec.
wavformat audiospec_to_wavformat(SDL_AudioSpec * from)
{
    auto isfloat = SDL_AUDIO_ISFLOAT(from->format);
    auto bytes = SDL_AUDIO_BITSIZE(from->format)/8;
    auto isbig = bytes > 2 and !isfloat;
    return
    { (bool)isfloat
    , (Uint16)from->channels
    , (Uint32)from->freq
    , (Uint16)(bytes*from->channels)
    , (Uint8)bytes
    , (float)(isfloat?1.0f:isbig?0.0f:ipower(0x100,bytes)/2.0f)
    , (double)(isbig?ipower(0x100,bytes)/2.0L:0.0L)
    , 1.0f
    };
}

// Get a correctly formatted sample from a place in RAM
// This is like a specialized casting function.
// There are three general kinds of sample:
// Unsigned integer (0 == -1.0)
// Signed integer (0 == 0.0)
// Floating point (-1.0 ~ 1.0)
// We support the following kinds:
// Unsigned int: 8-bit
// Signed int: 16-bit, 24-bit, 32-bit
// Float: 32-bit (float), 64-bit (double)  FIXME: 64-bit float is not actually currently handled!
//  WE ALSO HANDLE SAMPLE-DOMAIN VOLUME IN THIS FUNCTION.
float get_sample(void * addr, wavformat * fmt)
{
    float trans;
    if(fmt->bytespersample == 1)
    {
        trans = (int)(*(Uint8*)addr)-0x80;
        trans /= fmt->datagain;
    }
    if(fmt->bytespersample == 2)
    {
        trans = *(Sint16*)addr;
        trans /= fmt->datagain;
    }
    if(fmt->bytespersample == 3)
    {
        Sint32 trans2 = *((Sint32*)addr);
        trans2 = trans2>>8<<8 / 256;
        trans = (double)(trans2) / fmt->slowdatagain;
    }
    if(fmt->bytespersample == 4)
    {
        if(fmt->isfloatingpoint)
            trans = *(float*)addr;
        else
        {
            double trans2 = *((Sint32*)addr);
            trans = trans2 / fmt->slowdatagain;
        }
    }
    return trans * fmt->volume;
}

// The inverse of the above function.
//  WE DO NOT HANDLE SAMPLE-DOMAIN VOLUME IN THIS FUNCTION.
void set_sample(void * addr, wavformat * fmt, float val)
{
    if(fmt->bytespersample == 1)
    {
        *(Uint8*)addr = val*fmt->datagain+0x80; // TODO: Do I have to overflow-check this?
        return;
    }
    if(fmt->bytespersample == 2)
    {
        *(Sint16*)addr = val*fmt->datagain;
        return;
    }
    if(fmt->bytespersample == 3)
    {
        Uint32 inter = (double)val*fmt->slowdatagain;
        inter = inter&0xFFFFFF00;
        auto inter2 = (Uint8*)&inter;
        *((Uint8*)(addr)+0) = *(inter2+1); // TODO: There has got to be a better way to do this.
        *((Uint8*)(addr)+1) = *(inter2+2); 
        *((Uint8*)(addr)+2) = *(inter2+3); 
        return;
    }
    if(fmt->bytespersample == 4)
    {
        if(fmt->isfloatingpoint)
            *(float*)addr = val;
        else
            *(Uint32*)addr = val * fmt->slowdatagain;
        return;
    }
    puts("ayy");
}
