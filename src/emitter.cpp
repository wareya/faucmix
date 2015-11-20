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
void * emitter::generateframe(SDL_AudioSpec * spec, unsigned int len)
{
    // Skip if not ready (fallback case)
    if(!info.playing or !stream->ready())
    {
        return nullptr;
    }
    
    // Generate simple frame from stream if possible
    if(stream->channels() == spec->channels)
    {
        return stream->generateframe(spec, len, &info);
    }
    // If we have to channel convert, we simply do that
    else
    {
        SDL_AudioSpec temp = *spec;
        temp.channels = stream->channels();
        // Generate native-channeled frame from stream
        auto badbuffer = stream->generateframe(&temp, len*temp.channels/spec->channels, &info);
        auto goodlen = len;
        
        // Reallocate scratch buffer if needed
        if(goodlen > DSPlen and DSPbuffer != nullptr)
        {
            free(DSPbuffer);
            DSPbuffer = nullptr;
        }
        if(DSPbuffer == nullptr)
        {
            std::cout << goodlen << "\n";
            DSPbuffer = malloc(goodlen);
            DSPlen = goodlen;
        }
        // Convert native-channeled stream into output-channeled stream
        int r = channel_cvt(DSPbuffer, badbuffer, len/spec->channels/(SDL_AUDIO_BITSIZE(spec->format)/8), spec, temp.channels);
        return DSPbuffer;
    }
    
}
void emitter::fire()
{
    stream->fire(&info);
}
void emitter::cease()
{
    stream->cease(&info);
}
