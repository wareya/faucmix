#include <SDL2/SDL.h>
#include <stdio.h>
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
    int channels;
    int samplerate;
    virtual void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info);
    virtual bool isfinished();
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
    Uint8 bytespersample;
    float datagain;
    double slowdatagain;
};

struct wavstream : pcmstream
{
    std::atomic<bool> ready;
    
    Uint8 * fmt = nullptr;
    Uint8 * data = nullptr;
    Uint32 length;
    
    std::string stored;
    wavformat format;
    
    Uint8 * buffer = nullptr;
    Uint32 bufferlen;
    
    wavstream(const char * filename)
    {
        ready = false;
        stored = std::string(filename);
        SDL_CreateThread(&t_wavfile_load, "faucetmix2.cpp:t_wavfile_load", this);
    }
    
    Uint32 position; // Position is in OUTPUT SAMPLES, not SOUNDBYTE SAMPLES, i.e. it counts want.freqs not wavstream.freqs
    void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info)
    {
        if(buffer == nullptr)
            buffer = (Uint8 *)malloc(spec->channels*SDL_AUDIO_MASK_BITSIZE(spec->format)/8*len);
        return nullptr;
    }
    bool isplaying()
    {
        return false;
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
    auto self = (wavstream *)etc;
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
            printf("Unknown chunk 0x%08X, seeking by 0x%08X\n", *(Uint32*)(subchunk), len);
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
    self->length = length;
    self->fmt = fmt;
    self->data = data;
    self->ready = true;
    return GOOD;
}

struct emitter
{
    pcmstream * stream;
    emitterinfo info;
    void * generateframe(SDL_AudioSpec * spec, unsigned int len);
};
void * emitter::generateframe(SDL_AudioSpec * spec, unsigned int len)
{
    if(!info.playing)
        return nullptr;
    return stream->generateframe(spec, len, &info);
}


void respondtoSDL(void * udata, Uint8 * stream, int len)
{
    auto& spec = *(SDL_AudioSpec *)udata;
    int stream_bitdepth = SDL_AUDIO_BITSIZE(spec.format);
    int stream_datagain = ipower(2, stream_bitdepth)/2; // note: fails on unsigned or float output
    int channels = spec.channels;
    int used = 0;
    
    int stream_LE = SDL_AUDIO_ISLITTLEENDIAN(spec.format);
    
    std::vector<void *> responses;
    //for(auto stream : streamlist)
    //{
    //    responses.push_back(stream->generateframe(stream_bitdepth, spec, len));
    //}
    while(used < len)
    {
        // TODO: mix responses
        len++;
    }
}

int main(int argc, char * argv[])
{
    if(argc == 1)
        return 0 & puts("Usage: program file");
    wavstream sample(argv[1]);
    
    emitter output;
    output.stream = &sample;
    //output.position = 0;
    output.info.pan = 0.0f;
    output.info.volume = 1.0f;
    output.info.playing = true;
    output.info.loop = true;
    output.info.mixdown = 1.0f;
    
    //emitters.push_back(&output);
    SDL_AudioSpec want;
    want.freq = 44100;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 1024;
    want.callback = respondtoSDL;
    want.userdata = &want;
    SDL_AudioSpec got;
    SDL_OpenAudio(&want, &got);
    
    printf("%d\n", got.freq);
    
    SDL_PauseAudio(0);
    //while(output.playing)
    //    SDL_Delay(10);
    
    return 0;
}
