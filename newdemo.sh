#!/usr/bin/env bash
g++ -DNO_OPUS -std=c++11 src/*.cpp -I../kotareci/dependencies/sdl2-include/ -L../kotareci/dependencies/sdl2-lib/ \
-static \
-s -g -ggdb \
-Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors \
-static-libstdc++ \
$(../kotareci/dependencies/sdl2-bin/sdl2-config --static-libs --cflags | sed s.-lSDL2main.. | sed s.-Dmain=SDL_main..) \
-mwindows -mconsole \
-o fauxmix.exe
#ar rvs libfauxmix.a fauxmix.o
