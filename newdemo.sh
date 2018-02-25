#!/usr/bin/env bash
g++ -DNO_OPUS -std=c++14 src/*.cpp \
-static \
-s -g -ggdb \
-Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors \
-static-libstdc++ \
-lavrt -lole32 -lksuser -ldsound -ldxguid \
-mwindows -mconsole \
-Os -ffunction-sections -flto -fwhole-program -Wl,--gc-sections -Wl,--strip-all \
-o fauxmix.exe
#ar rvs libfauxmix.a fauxmix.o
