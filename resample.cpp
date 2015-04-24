#include "resample.hpp"

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
            for(auto s = 0; s < tgts; s++)
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
        for(auto s = 0; s < tgts; s++)
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
        
        for(auto s = 0; s < tgts; s++)
        {
            auto srcrate = srcfmt->samplerate;
            auto tgtrate = tgtfmt->samplerate;
            Sint64 which = position+s;
            
            // Fun fact: you can use integer division as a kind of floor/ceil!
            Sint32 window_bottom = (which*srcrate-srcrate)/tgtrate + ((which%tgtrate != 0)?1:0); // that addition part is the ceil
            Sint32 window_top = (which*srcrate+srcrate)/tgtrate;
            int window_length = window_top - window_bottom;
            //printf("%d %d %d\n", window_bottom, window_top, window_length);
            
            Uint64 hiorder_position = which * srcrate;
            Uint64 hiorder_bottom = hiorder_position - (window_bottom * tgtrate);
            Uint64 hiorder_winlen = 2*srcrate;
            
            for(auto c = 0; c < channels; c++)
            {
                float transient = 0.0f;
                float calibrate = 0.0f;
                for(auto i = 0; i <= window_length; i++) // convolution
                {
                    if((window_bottom+i) > srcs or (window_top+i) < 0)
                        continue;
                    Uint32 hiorder_closeness = abs(i*tgtrate - hiorder_bottom - tgtrate);
                    float closeness = double((Sint64)srcrate - hiorder_closeness) / srcrate;
                    transient += get_sample((Uint8*)src + ((window_bottom+i)*srcfmt->channels+c)*srcb, srcfmt) * closeness;
                    calibrate += closeness;
                }
                //std::cout << calibrate << " " << (float)srcrate/tgtrate << "\n";
                transient /= calibrate;
                set_sample((Uint8*)tgt+(s*tgtfmt->channels+c)*tgtb, tgtfmt, transient);
            }
        }
        /*
        float point = ratefactor*emitter->position; // point is position on emitter stream
        int bottom = ceil(point-ratefactor); // window
        int top = floor(point+ratefactor);
        float windowlen = ratefactor*2;
        
        float calibrate = 0; // convolution normalization
        float sample = 0; // output sample
        for(float j = ceil(bottom); j < top; j++) // convolution
        {
            float factor = j>point?j-point:point-j; // distance from output sample
            factor = ratefactor - factor; // convolution index of this sample
            calibrate += factor;
            sample += emitter->sample->sample_from_channel_and_position(i, j) * factor;
        }
        sample /= calibrate;
        transient += sample;*/
    }
}
