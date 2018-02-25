#include "wavfile.hpp"

#include <stdio.h>
#include <thread>
#include <new> // std::nothrow

void t_wavfile_load(wavfile * self);

wavfile * wavfile_load(const char * filename)
{
    puts("loading sample");
    auto sample = new (std::nothrow) wavfile;
    if(sample)
    {
        sample->status = 0;
        sample->stored = std::string(filename);
        std::thread mythread(t_wavfile_load, sample);
        mythread.detach();
    }
    return sample;
}

float get_sample(void * addr, wavformat * fmt)
{
    float trans = 0;
    if(fmt->bytespersample == 1)
    {
        trans = (int(*(uint8_t*)addr))-0x80;
        trans /= fmt->datagain;
    }
    if(fmt->bytespersample == 2)
    {
        trans = *(int16_t*)addr;
        trans /= fmt->datagain;
    }
    if(fmt->bytespersample == 3)
    {
        int32_t trans2 = *((int32_t*)addr);
        trans2 = (trans2&0xFFFFFF00)>>8;
        trans = (double)(trans2) / fmt->slowdatagain;
    }
    if(fmt->bytespersample == 4)
    {
        if(fmt->isfloatingpoint)
            trans = *(float*)addr;
        else
        {
            double trans2 = *((int32_t*)addr);
            trans = trans2 / fmt->slowdatagain;
        }
    }
    return trans;
}

int inner_wavfile_load(wavfile * self);

void t_wavfile_load(wavfile * self)
{
    self->error = inner_wavfile_load(self);
}

int inner_wavfile_load(wavfile * self)
{
    enum returncodes
    {
        GOOD,
        COULNOTOPEN,
        IO_ERROR,
        ALLOCATION_ERROR,
        INVALID_FORMAT,
        UNSUPPORTED_FORMAT
    };
    
    uint8_t * fmt = nullptr;
    uint64_t fmtlen = 0;
    uint8_t * data = nullptr;
    uint64_t datalen = 0;
    
    auto cleanup = [fmt, data]() -> int
    {
        if(fmt)
            free(fmt);
        if(data)
            free(data);
    };
    
    goto pass;
    
    couldnotopen:
        cleanup();
        self->status = -1;
        return COULNOTOPEN;
    invalid:
        cleanup();
        self->status = -1;
        return INVALID_FORMAT;
    unsupported:
        cleanup();
        self->status = -1;
        return UNSUPPORTED_FORMAT;
    ioerror:
        puts("Error while reading file");
        cleanup();
        self->status = -1;
        return IO_ERROR;
    allocerror:
        puts("Failed to allocate memory!");
        cleanup();
        self->status = -1;
        return ALLOCATION_ERROR;
    
    pass:
    
    const char * fname = self->stored.data();
    auto file = fopen(fname, "rb");
    if (file == NULL)
    {
        goto couldnotopen;
    }
    
    fseek(file, 0, SEEK_END);
    auto filesize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if(filesize < 0x2C) // this is the smallest valid .wav filesize
    {
        puts("File too small to be valid wav!");
        goto invalid;
    }
    
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
        goto invalid;
    }
    if(!cmp(header+8, "WAVE")) //'WAVE'
    {
        puts("Not a WAVE file!");
        goto invalid;
    }
    
    /* read data from file */
    
    while(!ferror(file) and !feof(file))
    {
        uint32_t subchunk;
        fread(&subchunk, 4, 1, file);
        switch(subchunk)
        {
        case 0x20746d66: // 'fmt '
            fread(&fmtlen, 4, 1, file);
            fmt = (uint8_t *)malloc(fmtlen);
            if(!fmt)
            {
                goto allocerror;
            }
            fread(fmt, 1, fmtlen, file);
            if(ferror(file) or feof(file))
            {
                goto ioerror;
            }
            if(data)
                goto out; // double break pls
            break;
        case 0x61746164: //'data'
            fread(&datalen, 4, 1, file);
            data = (uint8_t *)malloc(datalen);
            if(!data)
            {
                goto allocerror;
            }
            fread(data, 1, datalen, file);
            if(ferror(file) or feof(file))
            {
                goto ioerror;
            }
            if(fmt)
                goto out; // double break pls
            break;
        default:
            uint32_t len;
            fread(&len, 4, 1, file);
            if (len%2) // yes there's software broken enough to output files with unaligned chunk sizes
                len++;
            printf("Unknown chunk 0x%08X, seeking by 0x%08X\n", subchunk, len);
            fseek(file, len, SEEK_CUR);
            if(ferror(file) or feof(file) or ftell(file) >= filesize)
                goto out; // double break pls
        }
        puts("loading");
    }
    out:
    
    fclose(file);
    
    if(!fmt or !data)
    {
        goto invalid;
    }
    
    if(fmtlen != 16)
    {
        puts("Format appears to be compressed.");
        goto unsupported;
    }
    
    auto sampleformat = *(uint16_t*)(fmt);
    auto channels =  *(uint16_t*)(fmt+2);
    
    if(channels != 1 and channels != 2)
    {
        puts("WAVE file uses more than two channels. Not supported.");
    }
    
    auto samplerate =  *(uint32_t*)(fmt+4);
    // uint32 byterate goes here but is unnecessary
    auto blocksize =  *(uint16_t*)(fmt+12);
    auto bitspersample =  *(uint16_t*)(fmt+14);
    
    bool isfloatingpoint = false;
    switch(sampleformat)
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
        goto invalid;
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
        goto invalid;
    }
    
    if(isfloatingpoint and bytespersample != 8 and bytespersample != 4)
    {
        puts("Bad floating point format!");
        goto invalid;
    }
    
    if(channels*bytespersample != blocksize)
    {
        puts("Invalid blocksize!");
        goto invalid;
    }
    
    /*
    bool isfloatingpoint;
    uint16_t channels;
    uint32_t samplerate;
    uint8_t bytespersample;
    float datagain;
    double slowdatagain;
    
    uint32_t length;
    uint8_t * fmt;
    uint8_t * data;
    */
    
    wavformat format;
    format.isfloatingpoint = isfloatingpoint;
    format.channels = channels;
    format.samplerate = samplerate;
    format.bytespersample = bytespersample;
    format.datagain = datagain;
    format.slowdatagain = slowdatagain;
    format.blocksize = blocksize;
    
    // copy to float buffer
    uint64_t length = datalen/blocksize;
    float * buffer = (float *)malloc(length*channels*sizeof(float));
    for(uint64_t i = 0; i < length*channels; i++)
    {
        float sample = get_sample(data + i*bytespersample, &format);
        buffer[i] = sample;
    }
    free(data);
    free(fmt);
    
    self->samples = length;
    self->channels = channels;
    self->samplerate = samplerate;
    self->buffer = buffer;
    self->status = 1;
    return GOOD;
}
