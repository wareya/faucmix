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

void * emitter::generateframe(SDL_AudioSpec * spec, unsigned int len)
{
    if(!info.playing or !stream->ready())
    {
        return nullptr;
        puts("nullptr");
    }
    
    if(stream->channels() == spec->channels)
    {
        return stream->generateframe(spec, len, &info);
    }
    else
    {
        SDL_AudioSpec temp = *spec;
        temp.channels = stream->channels();
        auto badbuffer = stream->generateframe(&temp, len*temp.channels/spec->channels, &info);
        auto goodlen = len;
        
        if(DSPlen != goodlen and DSPbuffer != nullptr)
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
