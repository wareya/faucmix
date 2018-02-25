#include "resample.hpp"

#include <iostream>
#include <math.h>
#include <string.h>

// Below is black magic of integer sample progress math resampling.
// Ye who enters: Please understand DSP.
// The function is a triangle filter.

// Takes N channels, gives N channels
// TODO: Integer hermite?
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
)
{
    enum returncodes
    {
        NORESAMPLING,
        UPSAMPLED,
        DOWNSAMPLED
    };
    
    int64_t difference = int64_t(targetfreq) - int64_t(sourcefreq);
    
    memset(target, 0, targetlen*channels*sizeof(float));
    
    if(difference == 0)
    {
        position = position % sourcelen;
        for(uint64_t i = 0; i < targetlen; i++)
        {
            for(uint64_t c = 0; c < channels; c++)
            {
                if(looparound)
                    target[i*channels+c] = source[((position+i)%sourcelen)*channels+c];
                else if(position+i <= sourcelen)
                    target[i*channels+c] = source[(position+i)*channels+c];
                else
                    break;
            }
            return NORESAMPLING;
        }
    }
    
    else if (difference > 0) // upsample, use triangle filter to artificially create SUPER RETRO SOUNDING highs
    {
        for(uint64_t i = 0; i < targetlen; i++)
        {
            uint64_t which = position+i;
            // f = (x%r)/r (needs real num precision for high numbers) == (bx%a)/a (only needs real num precision at the last value)
            auto hiorder_phase = (which*sourcefreq) % targetfreq;
            float fraction = (float)hiorder_phase / targetfreq;
            
            auto lower = which*sourcefreq/targetfreq; // integer division performs truncation
            if(lower > sourcelen)
            {
                if(looparound)
                    lower = lower % sourcelen;
                else
                    // FIXME: wrong
                    break;
            }
            auto higher = lower;
            if(hiorder_phase > 0)
                higher = lower + 1;
            if(higher > sourcelen)
            {
                if(looparound)
                    higher = higher % sourcelen;
                else
                    // FIXME: wrong
                    break;
            }
            for(auto c = 0; c < channels; c++)
            {
                auto sample1 = source[lower*channels + c];
                auto sample2 = source[higher*channels + c];
                
                float out = sample1*(1.0-fraction) + sample2*fraction;
                
                target[i*channels + c] = out;
            }
        }
    }
    
    
    else // difference > 0
    {   // downsample, use triangle filter for laziness's sake, should really use sinc though
        // convert input position to surrounding output positions
        for(uint64_t s = 0; s < targetlen; s++)
        {
            uint64_t which = position+s;
            
            int64_t temp1 = (int64_t(which*sourcefreq)+int64_t(sourcefreq));
            int64_t temp2 = (int64_t(which*sourcefreq)-int64_t(sourcefreq));
            int64_t window_bottom = temp2/targetfreq + ((temp2%targetfreq)?1:0); // FIXME: what if temp2 is negative?
            int64_t window_top = temp1/targetfreq;
            int window_length = window_top - window_bottom;
            
            uint64_t hiorder_position = which * sourcefreq;
            uint64_t hiorder_bottom = (window_bottom * targetfreq);
            uint64_t hiorder_winlen = 2*sourcefreq;
            uint64_t hiorder_winheight = sourcefreq;
            
            for(auto c = 0; c < channels; c++)
            {
                float transient = 0.0f;
                float calibrate = (float)sourcefreq/targetfreq;
                for(auto j = 0; j <= window_length; j++) // convolution
                {
                    // location of sample in source stream
                    auto pickup = (window_bottom+j);
                    if((window_top+j) < 0)
                        continue;
                    if(pickup > sourcelen)
                    {
                        if(!looparound)
                            break; // FIXME: wrong
                        else while(pickup > sourcelen)
                            pickup -= sourcelen;
                    }
                    // "high order" (samplerate1*samplerate2 resolution) distance between pickup and center of kernel/window (on source stream)
                    int64_t hiorder_distance = int64_t(j*targetfreq+hiorder_bottom) - hiorder_position;
                    if(hiorder_distance < 0)
                        hiorder_distance = -hiorder_distance;
                    
                    if(hiorder_distance > hiorder_winheight) // failsafe // FIXME: wrong
                        continue;
                    uint64_t hiorder_closeness = hiorder_winheight - hiorder_distance;
                    
                    double closeness = hiorder_closeness/(float)hiorder_winheight;
                    transient += source[pickup*channels + c] * closeness;
                }
                transient /= calibrate;
                target[s*channels + c] = transient;
            }
        }
    }
}
