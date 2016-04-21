cat `echo src/*.cpp|sed s-src/newmain.cpp--` | \
g++ -std=c++11 -x c++ - -Isrc \
-static `pkg-config opusfile sdl2 --static --libs --cflags|sed s/-XCClinker//` \
-s -g -ggdb \
-Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors \
 -static-libstdc++ \
-shared -o fauxmix.dll
#ar rvs libfauxmix.a fauxmix.o
