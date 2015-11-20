#include "channels.hpp"
#include "format.hpp"

// writes n width-byte-wide values to tgt
int memwrite(void * tgt, Uint64 value, size_t n, int width)
{
    Uint64 mask = 0;
    switch(width)
    {
    case 1:
        mask = 0xFF;
        break;
    case 2:
        mask = 0xFFFF;
        break;
    case 3:
        mask = 0xFFFFFF;
        break;
    case 4:
        mask = 0xFFFFFFFF;
        break;
    case 5:
        mask = 0xFFFFFFFFFF;
        break;
    case 6:
        mask = 0xFFFFFFFFFFFF;
        break;
    case 7:
        mask = 0xFFFFFFFFFFFFFF;
        break;
    case 8:
        mask = 0xFFFFFFFFFFFFFFFF;
        break;
    }
    value = value & mask;
    for(auto i = 0; i < n*width; i += width)
    {
        for(auto j = 0; j < width; j++)
            ((Uint8*)tgt)[i,j] = ((Uint8*)&value)[j]; // [i,j]?????????
    }
}


// srcbuffer must be smaller than tgtbuffer in *samples*
// ONLY CONVERTS TO/FROM MONO.
int channel_cvt(void * tgtbuffer, void * srcbuffer, Uint32 samples, SDL_AudioSpec * tgtspec, Uint16 srcchannels)
{
    enum
    {
        UNSUPPORTED,
        INVALID,
        NOTHINGTODO,
        UNKNOWN,
        UP,
        DOWN
    } returncodes;
    
    auto tgtchannels = tgtspec->channels;
    
    /* Supported modes:
       mono -> n
       n -> mono
       up to 16 channels
    */
    
    if ( srcchannels == tgtchannels )
        return NOTHINGTODO;
    if ( srcchannels == 0 or tgtchannels == 0 )
        return INVALID;
    if ( srcchannels != 1 and tgtchannels != 1 )
        return UNSUPPORTED;
    if ( srcchannels > 16 or tgtchannels > 16 )
        return UNSUPPORTED;
    
    auto bitdepth = SDL_AUDIO_BITSIZE(tgtspec->format);
    if ( bitdepth % 8  or bitdepth == 0 )
        return UNSUPPORTED;
    
    auto bytedepth = bitdepth/8;
    
    if ( srcchannels == 1 )
    {   // duplicate source channels
        Uint64 mask = 0;
        switch(bytedepth)
        {
        case 1:
            mask = 0xFF;
            break;
        case 2:
            mask = 0xFFFF;
            break;
        case 3:
            mask = 0xFFFFFF;
            break;
        case 4:
            mask = 0xFFFFFFFF;
            break;
        case 8:
            mask = 0xFFFFFFFFFFFFFFFF;
            break;
        }/*
        if(SDL_AUDIO_ISSIGNED(tgtspec->format))
            memset(tgtbuffer, 0, samples*bytedepth*tgtchannels);
        else // unsigned
            memwrite(tgtbuffer, mask/2+1, samples*tgtchannels, bytedepth);*/
            //memset(tgtbuffer, 0x80, samples*bytedepth*tgtchannels);
        for(auto i = 0; i < samples; i++)
        {
            void * addr = (Uint8*)srcbuffer + i*bytedepth;
            Uint64 smpbuff = *(Uint64*)addr;
            for(auto c = 0; c < tgtchannels; c++)
            {
                for(auto j = 0; j < bytedepth; j++)
                    (((Uint8*)tgtbuffer)+(i*tgtchannels+c)*bytedepth)[j] = ((Uint8*)&smpbuff)[j];
            }
        }
        return UP;
    }
    // mix down to mono
    if ( tgtchannels == 1 )
    {
        wavformat fmt = audiospec_to_wavformat(tgtspec); // don't need to set/fix channels because get_sample won't use non-sampleformat info
        for(auto i = 0; i < samples; i++)
        {
            void * addr = (Uint8*)srcbuffer + i*bytedepth;
            float transient = 0.0f;
            for(auto c = 0; c < srcchannels; c++)
            {
                transient += get_sample((Uint8*)srcbuffer+(i*srcchannels+c)*bytedepth, &fmt);
            }
            transient /= srcchannels;
            set_sample((Uint8*)tgtbuffer+i*bytedepth, &fmt, transient);
        }
        return DOWN;
    }
    
    return UNKNOWN;
}
