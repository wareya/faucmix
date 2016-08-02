#ifndef NO_OPUS

#include "opusfile.hpp"

#include <opusfile.h>

#include <stdio.h>
#include <iostream>

// Shitty opus loading implementation that might break on malformed files
// I DID NOT SECURITY TEST THIS, DO NOT FEED UNTRUSTED OPUS FILES INTO THIS

wavfile * opusfile_load (const char * filename)
{
    puts("loading opus sample");
    auto sample = new wavfile;
    sample->status = 0;
    sample->stored = std::string(filename);
    sample->format.volume = 1.0f;
    SDL_CreateThread(&t_opusfile_load, "faucetmix2:t_opusfile_load", sample);
    return sample;
}

int t_opusfile_load(void * etc)
{
    auto self = (wavfile *)etc;
    const char * fname = self->stored.data();
    
    auto myfile = op_open_file(fname, NULL);
    if(myfile == NULL)
    {
        self->status = -1;
        puts("failed loading opus sample");
        return 0;
    }
    auto samples = op_pcm_total(myfile, -1);
    auto values = samples * 2;
    auto bytes = samples * 2 * 2;
    
    auto buffer = malloc(bytes);
    
    int prev, next;
    next = op_current_link(myfile);
    auto addr = (opus_int16*)buffer;
    std::cout << "links: " << op_link_count(myfile) << "\n";
    std::cout << "len: " << samples << "\n";
    do
    {
        prev = next;
        auto r = op_read_stereo(myfile, op_pcm_tell(myfile)*2+addr, values);
        if(r < 0)
        {
            self->status = -1;
            free(buffer);
            puts("failed loading opus sample");
            return 0;
        }
        else if (r == 0)
            break;
        
        next = op_current_link(myfile);
//        puts("chunk through");
//        std::cout << "read " << r << "\n";
    } while (prev != next or op_pcm_tell(myfile) < samples);
    
    std::cout << "prev " << prev << " next " << next << "\n";
    
    self->format.isfloatingpoint = false;
    self->format.channels = 2;
    self->format.samplerate = 48000;
    self->format.bytespersample = 2;
    self->format.datagain = 0x8000;
    self->format.slowdatagain = 0;
    self->format.blocksize = 4;
    
    unsigned char * fmt = (unsigned char*)malloc(16);
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
    self->data = (Uint8*)buffer;
    self->status = 1;
    puts("loaded opus sample");
}

#endif
