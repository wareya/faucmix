#include "opusfile.hpp"

#include <stdio.h>

wavfile * wavfile_load (const char * filename)
{
    puts("loading opus sample");
    auto sample = new wavfile;
    sample->status = 0;
    sample->stored = std::string(filename);
    SDL_CreateThread(&opusfile_load, "faucetmix2:t_opusfile_load", sample);
    return sample;
}

int t_opusfile_load(void * etc)
{
    auto self = (wavfile *)etc;
    const char * fname = self->stored.data();
    
    auto myfile = op_open_file(fname, void);
    if(myfile == NULL)
        return bad;
        
    auto samples = op_pcm_total(myfile);
    auto values = samples * 2;
    auto bytes = samples * 2 * 2;
    
    auto buffer = malloc(bytes);
    
    op_read_stereo(myfile, buffer, values);
    
    self->format.isfloatingpoint = false;
    self->format.channels = 2;
    self->format.samplerate = 48000;
    self->format.bytespersample = 2;
    self->format.datagain = 0x8000;
    self->format.slowdatagain = 0;
    self->format.blocksize = 4;
    self->format.volume = 1.0f;
    
    char * fmt = (char*)malloc(16);
    fmt[ 0] = 0x01;;
    fmt[ 1] = 0x00;
    fmt[ 2] = 0x02;
    fmt[ 3] = 0x00;
    fmt[ 4] = 0x80;;
    fmt[ 5] = 0xBB;
    fmt[ 6] = 0x00;
    fmt[ 7] = 0x00;
    fmt[ 8] = 0x00;;
    fmt[ 9] = 0xEE;
    fmt[10] = 0x02;
    fmt[11] = 0x00;
    fmt[12] = 0x04;;
    fmt[13] = 0x00;
    fmt[14] = 0x10;
    fmt[15] = 0x00;
    
    self->samples = samples;
    self->bytes = bytes;
    self->fmt = fmt;
    self->data = buffer;
    self->status = 1;
}
