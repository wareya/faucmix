#include "emitter.hpp"
#include "channels.hpp"
#include "wavstream.hpp"

#include <iostream>

emitter::emitter(wavfile * sample)
{
    stream = new wavstream(sample);
    info.playing = false;
}
emitter::~emitter()
{
    delete stream;
}

// Mixer asks for a frame
float * emitter::generateframe(uint64_t count, uint64_t channels, uint64_t samplerate)
{
    // Skip if not ready (fallback case)
    if(!info.playing or !stream->ready())
        return nullptr;
    
    return stream->generateframe(count, channels, samplerate, &info);
}
void emitter::fire()
{
    stream->fire(&info);
}
void emitter::cease()
{
    stream->cease(&info);
}
