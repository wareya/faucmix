#include "wavstream.hpp"
#include "resample.hpp"
#include "format.hpp"

wavstream::wavstream(const char * filename) // TODO: SEPARATE INSTANTIATION OF WAVSTREAM FROM WAVFILE
{
    puts("loading sample");
    sample.ready = false;
    sample.stored = std::string(filename);
    SDL_CreateThread(&t_wavfile_load, "faucetmix2.cpp:t_wavfile_load", &sample);
}
bool wavstream::ready()
{
    return sample.ready;
}
Uint16 wavstream::channels()
{
    return sample.format.channels;
}
void * wavstream::generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info)
{
    if(!sample.ready)
    {
        puts("not ready");
        return nullptr;
    }
    if(!sample.data)
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
    
    // TODO: CHECK FOR LOOP
    auto fuck = audiospec_to_wavformat(spec);
    linear_resample_into_buffer
    ( position
    , &sample.format
    , sample.data, sample.bytes
    , buffer, bufferlen
    , &fuck, false);
    
    position += len;
    
    if(position*(Uint64)sample.format.samplerate/spec->freq > sample.samples)
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
