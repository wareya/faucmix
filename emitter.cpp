#include "emitter.hpp"
#include "channels.hpp"

#include <iostream>

void * emitter::generateframe(SDL_AudioSpec * spec, unsigned int len)
{
    if(!info.playing or !stream->ready())
        return nullptr;
    
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
        #if 0
        if while do break emitter this "asdf" 0 sizeof
        #endif
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
        int r = channel_cvt(DSPbuffer, badbuffer, len, spec, temp.channels);
        return DSPbuffer;
    }
    
}
void emitter::fire()
{
    stream->fire(&info);
}
