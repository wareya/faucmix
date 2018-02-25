// -lole32 -ldsound -ldxguid
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdio.h>
#include <stdint.h>

typedef HRESULT (WINAPI * t_DirectSoundCreate8) (LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
t_DirectSoundCreate8 dyn_DirectSoundCreate8 = nullptr;
DSBUFFERDESC bufferdesc;
IDirectSoundBuffer * buffer_write;

#include <thread> // for sleeping

#include <math.h>

#define max(x,y) (((x)>(y))?(x):(y))

void dsound_loop_callback();

const auto samples = 1024;
const auto channels = 2;
const auto nbits = 16;
const auto nbytes = nbits/8;
const auto freq = 48000;

int dsound_init()
{
    // initialize COM
    CoInitialize(nullptr);
    
    IDirectSound8 * device;
    // FIXME: docs show &CLSID_DirectSound8, but I need to drop the & to compile, and it runs properly. what's going on?
    auto err = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectSound8, (LPVOID*) &device);
    if(err != S_OK)
        return puts("could not initialize directsound via COM\n"), 1;
    
    err = device->Initialize(&DSDEVID_DefaultPlayback);
    if(err != S_OK)
        return puts("failed to initialize dsound device\n"), 1;
    
    err = device->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
    if(err != S_OK)
        return puts("SetCooperativeLevel failed\n"), 1;
    
    // open up primary (underlying) buffer
    bufferdesc.dwSize = sizeof(bufferdesc);
    bufferdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    bufferdesc.dwBufferBytes = 0;
    bufferdesc.dwReserved = 0;
    bufferdesc.lpwfxFormat = 0;
    memset(&(bufferdesc.guid3DAlgorithm), 0, sizeof(bufferdesc.guid3DAlgorithm));
    
    IDirectSoundBuffer * buffer_out;
    
    err = device->CreateSoundBuffer(&bufferdesc, &buffer_out, 0);
    if(err != S_OK)
        return printf("CreateSoundBuffer (for buffer_out) failed %ld\n", err), 1;
    
    // desired format of output device (what happens on pre-WDM windows? this is like the format of the mixing stage in WASAPI)
    WAVEFORMATEX format;
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = freq;
    format.nAvgBytesPerSec = freq * nbytes * channels;
    format.nBlockAlign = nbytes * channels;
    format.wBitsPerSample = nbits;
    format.cbSize = 0;
    
    err = buffer_out->SetFormat(&format);
    if(err != S_OK)
        return printf("SetFormat failed %ld\n", err), 1;
    
    // open up secondary (ring buffer) buffer
    
    const auto oversize = 8; // does not affect latency, just underflow behavior
    
    bufferdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;
    bufferdesc.dwBufferBytes = samples * nbytes * channels * oversize;
    bufferdesc.lpwfxFormat = &format;
    
    // looping ring buffer to use to output to dsound's primary buffer
    err = device->CreateSoundBuffer(&bufferdesc, &buffer_write, 0);
    if(err != S_OK)
        return printf("CreateSoundBuffer (for buffer_write) failed %ld\n", err), 1;
    err = buffer_write->SetCurrentPosition(0);
    if(err != S_OK)
        return printf("SetCurrentPosition failed %ld\n", err), 1;
    
    // initialize ring buffer to silence
    DWORD clear_size; // must be DWORD, not uint32_t or uint64_t, due to those resolving to "unsigned int" and "long long unsigned int" on 32-bit mingw-w64
    void * clear_output;
    err = buffer_write->Lock(0, bufferdesc.dwBufferBytes, &clear_output, &clear_size, 0, 0, 0);
    if(err != S_OK)
        return printf("Lock failed %ld\n", err), 1;
    memset(clear_output, 0, clear_size);
    buffer_write->Unlock(clear_output, clear_size, 0, 0);
    if(err != S_OK)
        return printf("Unlock failed %ld\n", err), 1;
    
    err = buffer_write->Play(0, 0, DSBPLAY_LOOPING);
    if(err != S_OK)
        return printf("Play failed %ld\n", err), 1;
    
    return 0;
}

int dsound_destruct()
{
    // TODO
    // FIXME
}

extern float mixbuffer[4096*256];
void dsound_renderer(void callback (float * buffer, uint64_t count, uint64_t channels, uint64_t freq), void cleanup ())
{
    // demo output loop
    uint64_t c_write = 0;
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        DWORD c_out, c_read;
        auto err = buffer_write->GetCurrentPosition(&c_out, &c_read); // the docs name the cursors badly
        if(err != S_OK)
            printf("GetCurrentPosition threw error: %ld\n", err);
        
        // size of the whole ring buffer in bytes
        int64_t ringsize = bufferdesc.dwBufferBytes;
        
        int64_t badpad = int64_t(c_read) - int64_t(c_out);
        while(badpad < 0)
            badpad += ringsize;
        
        // write cursor's distance rightwards relative to read cursor
        int64_t rightwards = int64_t(c_write) - int64_t(c_read);
        while(rightwards < 0)
            rightwards += ringsize;
        
        int64_t write_ahead = int64_t(samples*nbytes*channels) - rightwards;
        // if we underflowed
        if(write_ahead < 0)
            write_ahead = int64_t(samples*nbytes*channels);
        // don't write more than half the ring buffer at once
        if(write_ahead > ringsize/2)
            write_ahead = ringsize/2;
        // mustn't write into this region of the ring buffer
        if(write_ahead > ringsize - badpad)
            write_ahead = ringsize - badpad;
        
        // how many sample frames to render
        uint64_t render = write_ahead/(nbytes*channels);
        if(render > 0)
        {
            callback(mixbuffer, render, channels, freq);
            
            // directsound ring buffers loop at the end and it might give us two buffers to write into when we lock it
            unsigned char * backend_buffer_1 = 0;
            DWORD backend_amount_1 = 0;
            unsigned char * backend_buffer_2 = 0;
            DWORD backend_amount_2 = 0;
            err = buffer_write->Lock(c_write, write_ahead, (void**)&backend_buffer_1, &backend_amount_1, (void**)&backend_buffer_2, &backend_amount_2, 0);
            if(err != S_OK)
                printf("Lock threw error: %ld\n", err);
            
            // write into the buffer (or two)
            uint64_t i = 0;
            uint64_t actually_written_1 = 0;
            for(uint64_t j = 0; backend_buffer_1 and j < backend_amount_1/nbytes and i < render*channels; j++)
            {
                ((int16_t*)(backend_buffer_1))[j] = round(mixbuffer[i++]*0x7FFF);
                actually_written_1 += nbytes;
            }
            uint64_t actually_written_2 = 0;
            for(uint64_t j = 0; backend_buffer_2 and j < backend_amount_2/nbytes and i < render*channels; j++)
            {
                ((int16_t*)(backend_buffer_2))[j] = round(mixbuffer[i++]*0x7FFF);
                actually_written_2 += nbytes;
            }
            
            // commit
            err = buffer_write->Unlock((void*)backend_buffer_1, actually_written_1, (void*)backend_buffer_2, actually_written_2);
            if(err != S_OK)
                printf("Unlock threw error: %ld; %ld %ld %ld\n", err, DSERR_INVALIDCALL, DSERR_INVALIDPARAM, DSERR_PRIOLEVELNEEDED);
            
            // progress write cursor by how much we actually wrote
            c_write += i*nbytes;
            c_write = c_write%ringsize;
            
            cleanup();
        }
    }
}
