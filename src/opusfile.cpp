#ifndef NO_OPUS

#include "opusfile.hpp"

#include <opusfile.h>

#include <stdio.h>
#include <math.h>

#include <thread>

// Shitty opus loading implementation that might break on malformed files
// SECURITY: NO SECURITY TESTING AT ALL. DO NOT FEED UNTRUSTED OPUS FILES INTO THIS.

int t_opusfile_load(wavfile * self);

wavfile * opusfile_load (const char * filename)
{
    puts("loading opus sample");
    auto sample = new wavfile;
    if(sample)
    {
        sample->status = 0;
        sample->stored = std::string(filename);
        std::thread mythread(t_opusfile_load, sample);
        mythread.detach();
    }
    return sample;
}

int t_opusfile_load(wavfile * self)
{
    const char * fname = self->stored.data();
    
    auto myfile = op_open_file(fname, NULL);
    if(myfile == NULL)
    {
        self->status = -1;
        return 0;
    }
    int64_t samples = op_pcm_total(myfile, -1);
    if(samples < 0)
    {
        self->status = -1;
        return 0;
    }
    uint64_t values = uint64_t(samples) * 2;
    
    float * buffer = (float *)malloc(values*sizeof(float));
    if(!buffer)
    {
        self->status = -1;
        return 0;
    }
    
    int prev, next;
    next = op_current_link(myfile);
    uint64_t values_left = values;
    do
    {
        prev = next;
        int64_t tell = op_pcm_tell(myfile);
        if(tell < 0)
        {
            self->status = -1;
            free(buffer);
            return 0;
        }
        int64_t r = op_read_float_stereo(myfile, buffer + tell*2, values_left);
        if(r < 0)
        {
            self->status = -1;
            free(buffer);
            return 0;
        }
        else if (r == 0)
            break;
        
        values_left -= r;
        
        next = op_current_link(myfile);
    } while (values_left > 0 and (prev != next or op_pcm_tell(myfile) < samples));
    
    self->samples = samples;
    self->channels = 2;
    self->samplerate = 48000;
    self->buffer = buffer;
    self->status = 1;
    
    op_free(myfile);
    
    puts("loaded opus file successfully");
}

#endif
