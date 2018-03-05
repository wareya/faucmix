#include "wavbuffer.hpp"

#include <stdlib.h> // size_t
#include <string.h> // memset

#define DEBUG_WAVBUFFER_ARRAYS

#ifdef DEBUG_WAVBUFFER_ARRAYS
#include <stdio.h> // puts
#include <signal.h> // raise
#endif

wavbuffer::wavbuffer(size_t frames, size_t chans)
{
    ultimate_length = frames*chans;
    samples = frames;
    channels = chans;
    array = nullptr;
    if(ultimate_length > 0)
        array = (float *)malloc(sizeof(float)*ultimate_length);
}
wavbuffer::wavbuffer()
{
    ultimate_length = 0;
    samples = 0;
    channels = 0;
    array = nullptr;
}
wavbuffer::~wavbuffer()
{
    if(array)
        free(array);
}


void wavbuffer::resize(size_t frames, size_t chans)
{
    ultimate_length = frames*chans;
    samples = frames;
    channels = chans;
    if(array)
        free(array);
    array = nullptr;
    if(ultimate_length > 0)
        array = (float *)malloc(sizeof(float)*ultimate_length);
}

void wavbuffer::clear()
{
    if(array)
        memset(array, 0, sizeof(float)*ultimate_length);
}

float & wavbuffer::operator[](ptrdiff_t i)
{
    #ifdef DEBUG_WAVBUFFER_ARRAYS
    if(i < 0)
    {
        puts("Negative index into wavbuffer");
        raise(SIGSEGV);
    }
    if(i >= ultimate_length)
    {
        puts("Too large index into wavbuffer");
        raise(SIGSEGV);
    }
    if(array == nullptr)
    {
        puts("Used index on nullptr array of wavbuffer");
        raise(SIGSEGV);
    }
    #endif
    return array[i];
}
