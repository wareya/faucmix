#include "wavfile.hpp"

#include <stdio.h>

wavfile * wavfile_load (const char * filename)
{
    puts("loading sample");
    auto sample = new wavfile;
    sample->status = 0;
    sample->stored = std::string(filename);
    sample->format.volume = 1.0f;
    SDL_CreateThread(&t_wavfile_load, "wavfile_load:t_wavfile_load", sample);
    return sample;
}

// TODO: Use return codes somehow. But it runs on a thread. Return to wrapper function that sets flags?
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
    
    Uint8 * fmt = nullptr;
    Uint32 fmtlen = 0;
    Uint8 * data = nullptr;
    Uint32 datalen = 0;
    
    
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
        Uint32 subchunk;
        fread(&subchunk, 4, 1, file);
        switch(subchunk)
        {
        case 0x20746d66: // 'fmt '
            fread(&fmtlen, 4, 1, file);
            fmt = (Uint8 *)malloc(fmtlen);
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
            data = (Uint8 *)malloc(datalen);
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
            Uint32 len;
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
    self->status = 1;
    puts("loaded sample");
    return GOOD;
}
