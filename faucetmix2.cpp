// Coypright 2015 Alexander "wareya" Nadeau <wareya@gmail.com>
// Available under either the ISC or zlib licenses.


#include <SDL2/SDL.h>
#undef main
#include <stdio.h>
#include <iostream>
#include <vector>
#include <atomic>
#include <string>

template <typename type>
Uint64 ipower(type b, type n)
{
    if(n == 0)
        return 1;
    if(n < 0)
        return 1/ipower(b,-n);
    Uint64 x = b;
    for(auto i = 1; i < n; i++)
        x *= b;
    return x;
}

struct emitterinfo
{
    float pan;
    float volume;
    bool loop;
    bool playing;
    float mixdown;
};

struct pcmstream
{
    int samplerate;
    virtual void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info)
    {}
    virtual bool isplaying()
    {}
    virtual bool ready()
    {}
    virtual Uint16 channels()
    {}
    virtual void fire(emitterinfo * info)
    {}
};

/*
// SKELETON CODE
struct pcmgenerator : pcmstream
{
    void * (
    void * generateframe(unsigned char bitdepth, unsigned char channels, unsigned int len, unsigned int freq, const emitterinfo * info)
    {
        
    }
    void
    bool isplaying()
    {
        
    }
}
*/

int t_wavfile_load(void * data);
struct wavformat
{
    bool isfloatingpoint;
    Uint16 channels;
    Uint32 samplerate;
    Uint16 blocksize;
    Uint8 bytespersample;
    float datagain;
    double slowdatagain;
};

wavformat audiospec_to_wavformat(SDL_AudioSpec * from)
{
/*
    float datagain = 0;
    double slowdatagain = 0;
    if(isfloatingpoint)
        datagain = 1.0f;
    else if(bytespersample == 1)
        datagain = 0x80;
    else if(bytespersample == 2)
        datagain = 0x8000;
    else if(bytespersample == 3)
        slowdatagain = 0x800000;
    else if(bytespersample == 4)
        slowdatagain = 0x80000000;
       */ 
        
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
    };
}

struct wavfile
{
    std::atomic<bool> ready;
    
    Uint8 * fmt = nullptr;
    Uint8 * data = nullptr;
    Uint32 samples;
    Uint32 bytes;
    
    std::string stored;
    wavformat format;
};

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
        if(fmt->isfloatingpoint)
            trans = *(float*)addr;
        else
        {
            trans = *(Sint16*)addr;
            trans /= fmt->datagain;
        }
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
            trans = *(double*)addr;
        else
        {
            double trans2 = *((Sint32*)addr);
            trans = trans2 / fmt->slowdatagain;
        }
    }
    return trans;
}

void set_sample(Uint8 * addr, wavformat * fmt, float val)
{
    if(fmt->bytespersample == 1)
    {
        *(Uint8*)addr = val*fmt->datagain+0x80; // TODO: Do I have to overflow-check this?
        return;
    }
    if(fmt->bytespersample == 2)
    {
        if(fmt->isfloatingpoint)
            *(float*)addr = val;
        else
            *(Sint16*)addr = val*fmt->datagain;
        return;
    }
    if(fmt->bytespersample == 3)
    {
        Uint32 inter = (double)val*fmt->slowdatagain;
        inter = inter&0xFFFFFF00;
        auto inter2 = (Uint8*)&inter;
        *(Uint8*)(addr  ) = *(inter2+1); // TODO: There has got to be a better way to do this.
        *(Uint8*)(addr+1) = *(inter2+2); 
        *(Uint8*)(addr+2) = *(inter2+3); 
        return;
    }
    if(fmt->bytespersample == 4)
    {
        if(fmt->isfloatingpoint)
            *(double*)addr = val;
        else
            *(Uint32*)addr = val * fmt->slowdatagain;
        return;
    }
    puts("ayy");
}

FILE * dumpfile;

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
        return INVALID;
    
    auto channels = srcfmt->channels;
    
    Sint64 difference = (Sint64)tgtfmt->samplerate - srcfmt->samplerate;
    auto srcs = srclen/srcfmt->blocksize;
    auto tgts = tgtlen/tgtfmt->blocksize;
    
    auto srcb = srcfmt->bytespersample;
    auto tgtb = tgtfmt->bytespersample;
    
    if(difference == 0)
    {
        puts("SAME");
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
        puts("UP");
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
    fwrite(tgt, 1, tgtlen, dumpfile);
    fflush(dumpfile);
}


struct wavstream : pcmstream
{
    wavfile sample;
    
    wavstream(const char * filename) // TODO: SEPARATE INSTANTIATION OF WAVSTREAM FROM WAVFILE
    {
        puts("loading sample");
        sample.ready = false;
        sample.stored = std::string(filename);
        SDL_CreateThread(&t_wavfile_load, "faucetmix2.cpp:t_wavfile_load", &sample);
    }
    
    Uint32 position; // Position is in OUTPUT SAMPLES, not SOUNDBYTE SAMPLES, i.e. it counts want.freqs not wavstream.freqs
    Uint8 * buffer = nullptr;
    Uint32 bufferlen;
    
    bool ready()
    {
        return sample.ready;
    }
    Uint16 channels()
    {
        return sample.format.channels;
    }
    void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info)
    {
        if(!sample.ready)
            return nullptr;
        
        if(!sample.data)
            return nullptr;
        
        Uint32 specblock = spec->channels*(SDL_AUDIO_BITSIZE(spec->format))/8; // spec blocksize
        if(specblock == 0)
            return nullptr;
        
        auto outputsize = len; // output bytes 
        len /= specblock; // output samples
        if(outputsize != bufferlen)
        {
            free(buffer);
            buffer = nullptr;
        }
        if(buffer == nullptr)
        {
            buffer = (Uint8 *)malloc(outputsize);
            bufferlen = outputsize;
        }
        
        // TODO: CHECK FOR LOOP
/*
int linear_resample_into_buffer
( Uint32 position
, wavformat * sourcebufferformat
, void * sourcebuffer
, Uint32 sourcebufferbytes
, void * targetbuffer
, Uint32 targetbufferbytes
, wavformat * targetbufferformat
, bool looparound
)
{*/
        auto fuck = audiospec_to_wavformat(spec);
        linear_resample_into_buffer
        ( position
        , &sample.format
        , sample.data, sample.bytes
        , buffer, bufferlen
        , &fuck, false);
        
        position += len;
        
        if(position*(Uint64)sample.format.samplerate/spec->freq > sample.samples)
            info->playing = false;
        
        return buffer;
    }
    void fire(emitterinfo * info)
    {
        position = 0;
        info->playing = true;
    }
};
int t_wavfile_load(void * etc)
{
    enum returncodes
    {
        GOOD,
        COULNOTOPEN,
        IO_ERROR,
        ALLOCATION_ERROR,
        INVALID_FORMAT,
        UNSUPPORTED_FORMAT,
        INVALID_BITSTREAM
    };
    /* setup */
    auto self = (wavfile *)etc;
    const char * fname = self->stored.data();
    
    auto file = fopen(fname, "rb");
    
    if (file == NULL)
        return COULNOTOPEN;
    fseek(file, 0, SEEK_END);
    auto filesize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if(filesize < 0x2C) // this is the smallest valid .wav filesize
        return INVALID_FORMAT;
    
    /* check header */
    
    char header[12];
    fread(header, 1, 12, file);
    
    auto cmp = [](const char * a, const char * b) -> int
    {
        if(std::string(b).length() != 4)
            return false;
        for(auto i:{0,1,2,3})
            if(a[i] != b[i])
                return false;
        return true;
    };
    
    if(!cmp(header, "RIFF")) //'RIFF'
    {
        puts("Not a RIFF file!");
        return INVALID_FORMAT;
    }
    if(!cmp(header+8, "WAVE")) //'WAVE'
    {
        puts("Not a WAVE file!");
        return INVALID_FORMAT;
    }
    
    /* read data from file */
    
    Uint8 * fmt = nullptr;
    Uint32 fmtlen = 0;
    Uint8 * data = nullptr;
    Uint32 datalen = 0;
    while(!ferror(file) and !feof(file))
    {
        Uint32 subchunk;
        fread(&subchunk, 4, 1, file);
        switch(subchunk)
        {
        case 0x20746d66: // 'fmt '
            fread(&fmtlen, 4, 1, file);
            fmt = (Uint8 *)malloc(fmtlen);
            if(!fmt)
            {
                puts("Failed to allocate memory!");
                return ALLOCATION_ERROR;
            }
            fread(fmt, 1, fmtlen, file);
            if(ferror(file) or feof(file))
            {
                puts("Error while reading file");
                return IO_ERROR;
            }
            if(data)
                goto out; // double break pls
            break;
        case 0x61746164: //'data'
            fread(&datalen, 4, 1, file);
            data = (Uint8 *)malloc(datalen);
            if(!data)
            {
                puts("Failed to allocate memory!");
                return ALLOCATION_ERROR;
            }
            fread(data, 1, datalen, file);
            if(ferror(file) or feof(file))
            {
                puts("Error while reading file");
                return IO_ERROR;
            }
            if(fmt)
                goto out; // double break pls
            break;
        default:
            Uint32 len;
            fread(&len, 4, 1, file);
            if (len%2) // yes there's software broken enough to output files with unaligned chunk sizes
                len++;
            printf("Unknown chunk 0x%08X, seeking by 0x%08X\n", subchunk, len);
            fseek(file, len, SEEK_CUR);
            if(ferror(file) or feof(file) or ftell(file) >= filesize)
                goto out; // double break pls
        }
    }
    out:
    
    fclose(file);
    
    if(!fmt or !data)
        return INVALID_FORMAT;
    
    if(fmtlen != 16)
    {
        puts("Format appears to be compressed.");
        return UNSUPPORTED_FORMAT;
    }
    
    auto format = *(Uint16*)(fmt);
    auto channels =  *(Uint16*)(fmt+2);
    auto samplerate =  *(Uint32*)(fmt+4);
    // uint32 byterate goes here but is unnecessary
    auto blocksize =  *(Uint16*)(fmt+12);
    auto bitspersample =  *(Uint16*)(fmt+14);
    
    bool isfloatingpoint = false;
    switch(format)
    {
    case 1: // integer PCM
        isfloatingpoint = false;
        break;
    case 3: // floating point PCM
        isfloatingpoint = true;
        break;
    case 0: // literally unknown
        return UNSUPPORTED_FORMAT;
    default: // any others
        return UNSUPPORTED_FORMAT;
    }
    
    if(bitspersample % 8)
    {
        puts("File uses fractional bytes per sample.");
        return INVALID_FORMAT;
    }
    auto bytespersample = bitspersample/8;
    
    float datagain = 0;
    double slowdatagain = 0;
    if(isfloatingpoint)
        datagain = 1.0f;
    else if(bytespersample == 1)
        datagain = 0x80;
    else if(bytespersample == 2)
        datagain = 0x8000;
    else if(bytespersample == 3)
        slowdatagain = 0x800000;
    else if(bytespersample == 4)
        slowdatagain = 0x80000000;
    else
    {
        puts("File is >32bits per sample. Consider slapping whoever encoded it.");
        return INVALID_FORMAT;
    }
    
    if(isfloatingpoint and bytespersample != 8 and bytespersample != 4)
    {
        puts("Bad floating point format!");
        return INVALID_FORMAT;
    }
    
    if(channels*bytespersample != blocksize)
    {
        puts("Invalid blocksize!");
        return INVALID_FORMAT;
    }
    
    /*
    bool isfloatingpoint;
    Uint16 channels;
    Uint32 samplerate;
    Uint8 bytespersample;
    float datagain;
    double slowdatagain;
    
    Uint32 length;
    Uint8 * fmt;
    Uint8 * data;
    */
    self->format.isfloatingpoint = isfloatingpoint;
    self->format.channels = channels;
    self->format.samplerate = samplerate;
    self->format.bytespersample = bytespersample;
    self->format.datagain = datagain;
    self->format.slowdatagain = slowdatagain;
    self->format.blocksize = blocksize;
    
    
    /* normalize float */
    Uint32 length = datalen/blocksize;
    if(isfloatingpoint)
    {
        float highest = 1.0;
        if(bytespersample == 4)
        {
            for(unsigned i = 0; i < length*channels; i++)
            {
                auto t = *(float*)&data[i*bytespersample];
                highest = t>highest?t:highest;
                highest = -t>highest?-t:highest;
            }
        }
        if(bytespersample == 8)
        {
            for(unsigned i = 0; i < length*channels; i++)
            {
                auto t = *(double*)&data[i*bytespersample];
                highest = t>highest?t:highest;
                highest = -t>highest?-t:highest;
            }
        }
        datagain = highest;
        printf("Normalized sample for %f peak amplitude.\n", datagain);
    }
    self->samples = length;
    self->bytes = datalen;
    self->fmt = fmt;
    self->data = data;
    self->ready = true;
    puts("loaded sample");
    return GOOD;
}

struct emitter
{
    pcmstream * stream;
    emitterinfo info;
    
    void * DSPbuffer = nullptr;
    size_t DSPlen;
    
    void * generateframe(SDL_AudioSpec * spec, unsigned int len);
    void fire();
};

// srcbuffer must be smaller than tgtbuffer in *samples*
int channel_cvt(void * tgtbuffer, void * srcbuffer, Uint32 samples, SDL_AudioSpec * tgtspec, Uint16 srcchannels)
{
    enum
    {
        UNSUPPORTED,
        INVALID,
        NOTHINGTODO,
        GOOD
    } returncodes;
    
    auto tgtchannels = tgtspec->channels;
    
    if ( srcchannels == 0 or tgtchannels == 0 )
        return INVALID;
    if ( ( srcchannels > 2 and tgtchannels != 1 )
      or ( srcchannels != 1 and tgtchannels > 2 ))
        return UNSUPPORTED;
    if ( srcchannels == tgtchannels )
        return NOTHINGTODO;
    /*
    if( srcchannels == 1 )
    {
        for(auto i = samples-1; i >= 0; i--)
        {
            for(auto  = 
        }
    }*/
}

void * emitter::generateframe(SDL_AudioSpec * spec, unsigned int len)
{
    if(!info.playing or !stream->ready())
        return nullptr;
    
    if(stream->channels() == spec->channels)
    {
        return stream->generateframe(spec, len, &info);
        puts("c");
    }
    else
    {
        printf("%d\n", spec->channels);
        puts("x");
        SDL_AudioSpec temp = *spec;
        temp.channels = stream->channels();
        auto badbuffer = stream->generateframe(spec, len, &info);
        auto goodlen = len*spec->channels*SDL_AUDIO_BITSIZE(spec->format)/8;
        if(DSPlen != goodlen and DSPbuffer != nullptr)
            free(DSPbuffer);
        if(DSPbuffer == nullptr)
        {
            DSPbuffer = malloc(goodlen);
            channel_cvt(DSPbuffer, badbuffer, len, spec, temp.channels);
        }
    }
    
}
void emitter::fire()
{
    stream->fire(&info);
}

std::vector<emitter *> emitters;

void respondtoSDL(void * udata, Uint8 * stream, int len)
{
    auto& spec = *(SDL_AudioSpec *)udata;
    int stream_bitdepth = SDL_AUDIO_BITSIZE(spec.format);
    int stream_datagain = ipower(2, stream_bitdepth)/2; // note: fails on unsigned or float output
    int channels = spec.channels;
    int used = 0;
    
    int stream_LE = SDL_AUDIO_ISLITTLEENDIAN(spec.format);
    
    std::vector<void *> responses;
    for(auto stream : emitters)
    {
        auto f = stream->generateframe(&spec, len);
        if(f != nullptr)
            responses.push_back(f);
    }
    //while(used < len)
    {
        if(responses.size()>0)
            memcpy(stream, responses[0], len);
        
        // TODO: mix responses
        //len++;
    }
}

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
    wavstream sample(argv[1]);
    
    dumpfile = fopen("here!.raw", "wb");
    
    emitter output;
    output.stream = &sample;
    output.info.pan = 0.0f;
    output.info.volume = 1.0f;
    output.info.playing = true;
    output.info.loop = true;
    output.info.mixdown = 1.0f;
    
    emitters.push_back(&output);
    
    SDL_AudioSpec want;
    want.freq = 16000;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 1024;
    want.callback = respondtoSDL;
    want.userdata = &want;
    SDL_AudioSpec got;
    auto r = SDL_OpenAudio(&want, &got);
    if(r < 0)
    {
        puts("Failed to open device");
        std::cout << SDL_GetError();
    }
    
    printf("%d\n", got.freq);
    
    output.fire();
    
    SDL_PauseAudio(0);
    puts("a");
    while(output.info.playing)
        SDL_Delay(10);
    
    return 0;
}
