# faucmix

A lightweight stereo positional audio library meant for indie games. Compatible
with C and Game Maker 8 (GMS not tested).

Unlike most permissively-licensed games audio libraries, you can update each
audio source independently, even if they share the same audio clip.

# License

You can use faucmix under the terms of either the ISC license or the zlib license.

# Interface Outline

fauxmix uses a push method for commands in order to avoid certain timing-related
problems. This means there's a certain function that you need to call every
single frame, even if nothing interesting is happening (no new events).

Most "event" functions schedule an update. If you try to read info about the
state that they're modifying, it actually reads from a shadow copy that was
updated the last time that event was run.

## Start up the library:

    void fauxmix_dll_init();
    // Initialize the library's internal state
    
    void fauxmix_use_float_output(boolean b);
    // Specify that you want the device to try to open with floating point output (should not be called after fauxmix_init)
    
    boolean fauxmix_init(number samplerate, boolean mono, number samples);
    // Opens the audio device and initializes other internal state. Can fail.
    // - samplerate: Ask SDL for a device with this samplerate. Almost never affects the driver samplerate.
    // - mono: If try, asks SDL for a monaural audio device. Asks for stereo otherwise.
    // - samples: Ask SDL to open a device that reads this many samples at once when it takes updates. Lower numbers = lower latency. 512, 1024, and 2048 are recommended.
    // Returns false on failure.
    
    void fauxmix_close();
    // Closes the SDL audio device. Does not necessarily shut down streams; if the audio device is reinitialized, they might continue where they left off.

## What you do every frame:

    void fauxmix_push();
    // Commits commands to the mixing thread.

## Loading new sounds:
    
    id fauxmix_sample_load(string filename);
    // Starts a thread to load an audio clip from disk into RAM and build a streaming structure around it.
    // Supports RIFF WAV (.wav) files, as well as OGG Opus (.opus) if the library was compiled with support enabled.
    errorcode fauxmix_sample_status(id sample); // Shadowed!
    // Return value indicates the state of a sample loading in the background.
    //  0 - Still loading
    //  1 - Successfully loaded and playable
    // -1 - Failed loading (IO or validity error) -- needs to be manually killed by game dev
    // -2 - Non-existent sample (bad ID)

    id fauxmix_emitter_create(id sample);
    // Makes a new emitter that references the given sample. Returns a dud if sample id is invalid.
    errorcode fauxmix_emitter_status(id emitter); // Shadowed!
    // 0 - not playing
    // 1 - playing

## Controlling sounds (commands):

    errorcode fauxmix_sample_volume(id sample, float volume);
    // Sets the volume of this sample for all emitters that use this sample.
    errorcode fauxmix_emitter_volumes(id emitter, float left, float right);
    // Changes the volume of an emitter. Graceful: interpolates between the old and new audio levels over a short period of time.
    errorcode fauxmix_emitter_loop(id emitter, boolean whether);
    // Tells the mixer whether this emitter should loop automatically. If set on 
    errorcode fauxmix_emitter_pitch(id emitter, float ratefactor);
    // Sets the playack rate of an emitter.
    errorcode fauxmix_emitter_fire(id emitter);
    // Tells the mixer that this emitter should begin playing.
    errorcode fauxmix_emitter_cease(id emitter);
    // Ungracefully stops the playback of an emitter.

## Killing unneeded sounds (commands):
    void fauxmix_sample_kill(id sample);
    void fauxmix_emitter_kill(id emitter);

## Info:

    number fauxmix_get_samplerate();
    // Get the samplerate that the SDL device really opened with.
    number fauxmix_get_channels();
    // Get the numebr of channels that the SDL device really opened with.
    number fauxmix_get_samples();
    // Get the number of samples per bite that the SDL device really opened with.
    
    boolean fauxmix_is_ducking(); // Shadowed!
    // Check whether the mixer is currently ducking (low-distortion amplitude limiter).

