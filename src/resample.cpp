#include "resample.hpp"

#include <iostream>
#include <math.h>
#include <string.h>
#define puts(x) {}

// Below is black magic of integer math resampling.
// Ye who enters: Please understand DSP.
// The function is a triangle filter.

// Takes N channels, gives N channels
int linear_resample_into_buffer
( Uint32 position
, wavformat * srcfmt
, void * src
, Uint32 srclen
, void * tgt
, Uint32 tgtlen
, wavformat * tgtfmt
, bool looparound
)
{
    puts("start resample");
    enum returncodes
    {
        NORESAMPLING,
        EXPANDED,
        BITCRUSHED,
        UPSAMPLED,
        DOWNSAMPLED,
        INVALID,
        UNSUPPORTED
    };
    
    if(srcfmt->channels != tgtfmt->channels)
    {
        puts("invalid channel configuration");
        return INVALID;
    }
    auto channels = srcfmt->channels;
    
    Sint64 difference = (Sint64)tgtfmt->samplerate - srcfmt->samplerate;
    auto srcs = srclen/srcfmt->blocksize;
    auto tgts = tgtlen/tgtfmt->blocksize;
    
    auto srcb = srcfmt->bytespersample;
    auto tgtb = tgtfmt->bytespersample;
    
    memset(tgt, 0, tgtlen);
    
    if(difference == 0)
    {
        puts("even");
        position = position % srcs;
        for(unsigned s = 0; s < tgts; s++)
        {
            puts("s");
            for(auto c = 0; c < srcfmt->channels; c++)
            {
                puts("c");
                Sint32 overrun = (position+s > srcs);
                if(overrun and !looparound)
                {
                    puts("overrun stop");
                    return NORESAMPLING;
                }
                int srcpos = (position+s) % srcs;
                auto whichfrom = srcpos*srcfmt->channels+c;
                auto whichto = s*tgtfmt->channels+c;
                size_t from = (size_t)src + whichfrom*srcb;
                size_t to = (size_t)tgt + whichto*tgtb;
                set_sample((Uint8*)to, tgtfmt, get_sample((Uint8*)from, srcfmt));
            }
        }
    }
    
    
    // TODO: Integer hermite possible?
    else if (difference > 0) // upsample, use triangle filter to artificially create SUPER RETRO SOUNDING highs
    {
        puts("over");
        for(unsigned s = 0; s < tgts; s++)
        {
            puts("s");
            auto srcrate = srcfmt->samplerate;
            auto tgtrate = tgtfmt->samplerate;
            Uint64 which = position+s;
            // f = (x%r)/r (needs real num precision for high numbers) == (bx%a)/a (only needs real num precision at the last value)
            auto moire = (which*srcrate) % tgtrate;
            float fraction = (float)moire / tgtrate;
            
            auto lower = which*srcrate/tgtrate; // integer division performs truncation
            if(lower > srcs)
            {
                if(looparound)
                    lower = lower % srcs;
                else
                    break;
            }
            auto higher = lower;
            if(moire > 0)
                higher = lower + 1;
            if(higher > srcs)
            {
                if(looparound)
                    higher = higher % srcs;
                else
                    break;
            }
            for(auto c = 0; c < srcfmt->channels; c++)
            {
                puts("c");
                auto sample1 = get_sample((Uint8*)src+((lower*srcfmt->channels+c)*srcb), srcfmt);
                auto sample2 = get_sample((Uint8*)src+((higher*srcfmt->channels+c)*srcb), srcfmt);
                
                float out = sample1*(1.0-fraction) + sample2*fraction;
                
                set_sample((Uint8*)tgt+(s*tgtfmt->channels+c)*tgtb, tgtfmt, out);
            }
        }
    }
    
    
    else // difference > 0
    {   // downsample, use triangle filter for laziness's sake
        // convert input position to surrounding output positions
        puts("under");
        for(unsigned s = 0; s < tgts; s++)
        {
            // FIXME: I have an automatic casting problem somewhere here that is covered up by the specific types I pick. I think I overflow and then recast. Rewrite?
            puts("s");
            auto srcrate = srcfmt->samplerate;
            auto tgtrate = tgtfmt->samplerate;
            Uint64 which = position+s;
            
            // Fun fact: you can use integer division as a kind of floor/ceil!
            Sint64 temp1 = (which*srcrate+srcrate);
            Sint64 temp2 = (which*srcrate-srcrate);
            Sint32 window_bottom = temp2/tgtrate + ((temp2%tgtrate)?1:0); // that addition part is the ceil
            Sint32 window_top = temp1/tgtrate;
            int window_length = window_top - window_bottom;
            //printf("%d %d %d\n", window_bottom, window_top, window_length);
            
            Uint64 hiorder_position = which * srcrate;
            Uint64 hiorder_bottom = (window_bottom * tgtrate);
            Uint64 hiorder_winlen = 2*srcrate;
            Uint64 hiorder_winheight = srcrate;
            
            for(auto c = 0; c < channels; c++)
            {
                puts("c");
                float transient = 0.0f;
                float calibrate = (float)srcrate/tgtrate;
                for(auto i = 0; i <= window_length; i++) // convolution
                {
                    puts("i");
                    auto pickup = (window_bottom+i);
                    if((window_top+i) < 0)
                        continue;
                    if(pickup > srcs)
                    {
                        puts("p");
                        if(!looparound)
                        {
                            break;
                        }
                        else
                        {
                            while(pickup > srcs)
                                pickup -= srcs;
                        }
                        puts("pickuping");
                    }
                    Sint32 hiorder_distance = (Sint64)(i*tgtrate+hiorder_bottom) - hiorder_position;
                    if(hiorder_distance < 0)
                        hiorder_distance = -hiorder_distance;
                    
                    if(hiorder_distance > hiorder_winheight) // failsafe
                    {
                        puts("fuck");
                        float ratefactor = (float)srcrate/tgtrate;
                        continue;
                    }
                    Uint32 hiorder_closeness = hiorder_winheight - hiorder_distance;
                    
                    double closeness = hiorder_closeness/(float)hiorder_winheight; // NOTE: Should "closeness" be done in hiorder space?
                    transient += get_sample((Uint8*)src + (pickup*srcfmt->channels+c)*srcb, srcfmt) * closeness;
                }
                transient /= calibrate;
                set_sample((Uint8*)tgt+(s*tgtfmt->channels+c)*tgtb, tgtfmt, transient);
            }
        }
    }
    puts("end resample");
}
