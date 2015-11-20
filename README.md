# faucmix
An audio mixer written from scratch on top of SDL2 without SDL2_mixer.

# License
You can use faucmix under the terms of either the ISC license or the zlib license.

# Architecture

There are two important aspects to faucmix's design:

* How the API communicates with the mixing thread
* How the mixing thread gets things done (concepts)

An image describing how the API communicates with the mixing thread is here: http://i.imgur.com/zxLwHx8.png

The mixing thread works as follows:

1. There's a callback running in SDL_audio's thread that generates samples for the audio driver
2. This callback has a list of live **emitters** and gets scratch buffers from them, which it then mixes together using mixer information, including panning and channel volume. (Note: double check that channel volume is implemented)
3. **Emitters** ask a **stream** for waveform data that is in the correct sample rate, but not necessarily the correct number of channels. The emitter then channelmixes the stream output. **Currently, only mono and stereo channel layouts are supported by this engine.**
4. **Streams** can be absolutely anything; Faucet mixer comes with a single kind of built in stream: wavstream. This kind of stream is capable of loading a .opus or .wav file into memory and outputting accurately **triangle filter** resampled, correctly looped data to its emitter.
5. **Wavstreams** happen to contain a pointer to a **wavfile**, which corresponds to the **samples** used in the API. **Wavstreams** are part of the **emitter** in API jargon, but they need to be separate in order to allow for certain kinds of extensibility. It makes no sense for an emitter to have a concept of an "audio file" when all an emitter needs to is handle audio frames.
