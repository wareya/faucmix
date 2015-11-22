cat `echo src/*.cpp|sed s-src/newmain.cpp--` | \
i686-w64-mingw32-g++ -std=c++11 -x c++ - -Isrc \
-static `i686-w64-mingw32-pkg-config opusfile sdl2 --static --libs --cflags|sed s/-XCClinker//` \
-s -g -ggdb \
-DGAME_MAKER \
-Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors \
 -static-libstdc++ \
-shared -o fauxmixgm.dll
#ar rvs libfauxmix.a fauxmix.o
