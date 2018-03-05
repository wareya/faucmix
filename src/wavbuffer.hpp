#ifndef INCLUDED_WAVBUFFER
#define INCLUDED_WAVBUFFER

#include <stdlib.h> // size_t

// Container for arrays of audio samples

struct wavbuffer {
    wavbuffer(size_t frames, size_t chans);
    wavbuffer();
    ~wavbuffer();
    void resize(size_t frames, size_t chans);
    void clear();
    float & operator[](ptrdiff_t i);
    
    wavbuffer(const wavbuffer &) = delete;
    wavbuffer & operator=(const wavbuffer &) = delete;
    
    size_t samples;
    size_t channels;
    size_t ultimate_length;
    float * array;
};


#endif
