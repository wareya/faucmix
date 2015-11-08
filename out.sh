cat `echo src/*.cpp` | \
g++ -fPIC -std=c++11 -x c++ - -Isrc \
`pkg-config opusfile sdl2 --libs --cflags|sed s/-XCClinker//` \
-s -g -ggdb \
-Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors \
-o fauxmix.out
