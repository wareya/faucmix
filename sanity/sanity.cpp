#include <SDL2/SDL.h>
#undef main

#include <iostream>
#include <stdio.h>

SDL_AudioSpec want, got;

void respondtoSDL(void * udata, Uint8 * stream, int len)
{
	for(auto i = 0; i < len; i += 1)
	{
		stream[i] = random()%0x100;
	}
}

int main()
{
	SDL_Init(SDL_INIT_AUDIO);
	std::cout << SDL_GetError() << "\n";
	
	SDL_AudioDeviceID dev;

	want.freq = 44100;
	want.format = AUDIO_S16;
	want.channels = 2;
	want.samples = 1024;
	want.callback = respondtoSDL;
	dev = SDL_OpenAudio(&want, &got);
	std::cout << SDL_GetError() << "\n";
    SDL_PauseAudio(0);
	std::cout << SDL_GetError() << "\n";
	SDL_Delay(1000);
}
