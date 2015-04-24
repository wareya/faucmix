#include "resample.hpp"

#include <iostream>

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
    
    if(difference == 0)
    {
        Sint32 overrun = tgts+position - srcs;
        overrun = overrun>0? overrun : 0;
        if(overrun)
            puts("overun");
        if( srcfmt->isfloatingpoint == tgtfmt->isfloatingpoint
        and srcfmt->bytespersample == tgtfmt->bytespersample)
        {
            memcpy(tgt, (char*)(src)+position*srcfmt->blocksize, overrun?tgtlen-overrun*tgtfmt->blocksize:tgtlen);
            //memcpy(tgt, (char*)(src)+position*srcfmt->blocksize, tgtlen);
            //memset((char*)tgt+position, (srcfmt->bytespersample==1)?128:0, overrun?overrun*tgtfmt->blocksize:0);
            return NORESAMPLING;
        }
        else // have to expand or crush 
        {
            for(unsigned s = 0; s < tgts; s++)
            {
                for(auto c = 0; c < srcfmt->channels; c++)
                {
                    auto whichfrom = (position+s)*srcfmt->channels+c;
                    auto whichto = s*tgtfmt->channels+c;
                    size_t from = (size_t)src + whichfrom*srcb;
                    size_t to = (size_t)tgt + whichto*tgtb;
                    set_sample((Uint8*)to, tgtfmt, get_sample((Uint8*)from, srcfmt));
                }
            }
        }
    }
    
    
    
    else if (difference > 0) // upsample, use triangle filter to artificially create SUPER RETRO SOUNDING highs
    {
        for(unsigned s = 0; s < tgts; s++)
        {
            auto srcrate = srcfmt->samplerate;
            auto tgtrate = tgtfmt->samplerate;
            Uint64 which = position+s;
            // f = (x%r)/r (needs real num precision for high numbers) == (bx%a)/a (only needs real num precision at the last value)
            auto moire = (which*srcrate) % tgtrate;
            float fraction = (float)moire / tgtrate;
            
            auto lower = which*srcrate/tgtrate; // integer division performs truncation
            auto higher = lower;
            if(moire > 0)
                higher = lower + 1;
            for(auto c = 0; c < srcfmt->channels; c++)
            {
                auto sample1 = get_sample((Uint8*)src+(lower*srcfmt->channels+c)*srcb, srcfmt);
                auto sample2 = get_sample((Uint8*)src+(higher*srcfmt->channels+c)*srcb, srcfmt);
                
                float out = sample1*(1.0-fraction) + sample2*fraction;
                
                set_sample((Uint8*)tgt+(s*tgtfmt->channels+c)*tgtb, tgtfmt, out);
            }
        }
    }
    
    
    else // difference > 0
    {   // downsample, use triangle filter for laziness's sake
        // convert input position to surrounding output positions
        
        for(unsigned s = 0; s < tgts; s++)
        {
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
                float transient = 0.0f;
                float calibrate = (float)srcrate/tgtrate;
                for(auto i = 0; i <= window_length; i++) // convolution
                {
                    if((window_bottom+i) > srcs or (window_top+i) < 0)
                        continue;
                    
                    Sint32 hiorder_distance = (Sint64)(i*tgtrate+hiorder_bottom) - hiorder_position;
                    if(hiorder_distance < 0)
                        hiorder_distance = -hiorder_distance;
                    
                    if(hiorder_distance > hiorder_winheight) // failsafe
                    {
                        puts("fuck");
                        printf("%d %d %d\n", window_bottom, window_top, i);
                        float ratefactor = (float)srcrate/tgtrate;
                        printf("%f\n", which*ratefactor - ratefactor);
                        printf("%ld %d %ld\n", which, tgtrate, (which*srcrate-srcrate)%tgtrate);
                        printf("%f\n", ceil(which*ratefactor - ratefactor));
                        printf("%f\n", floor(which*ratefactor + ratefactor));
                        printf("%ld %ld %d %d\n", (i*tgtrate+hiorder_bottom), hiorder_position, i, hiorder_distance);
                        continue;
                    }
                    Uint32 hiorder_closeness = hiorder_winheight - hiorder_distance;
                    
                    double closeness = hiorder_closeness/(float)hiorder_winheight;
                    transient += get_sample((Uint8*)src + ((window_bottom+i)*srcfmt->channels+c)*srcb, srcfmt) * closeness;
                }
                transient /= calibrate;
                set_sample((Uint8*)tgt+(s*tgtfmt->channels+c)*tgtb, tgtfmt, transient);
            }
        }
    }
}
