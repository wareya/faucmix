
struct pcmstream
{
    int samplerate;
    virtual void * generateframe(SDL_AudioSpec * spec, unsigned int len, emitterinfo * info);
    virtual bool isplaying();
    virtual bool ready();
    virtual Uint16 channels();
    virtual void fire(emitterinfo * info);
};
