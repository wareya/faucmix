g++ -fPIC -std=c++11 `echo src/*.cpp|sed s-src/newmain.cpp--` -Isrc \
`pkg-config opusfile sdl2 --libs --cflags|sed s/-XCClinker//` \
-g -ggdb \
-Wall -Wextra -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors \
-shared -fvisibility=hidden \
-o fauxmix.so
#ar rvs libfauxmix.a fauxmix.o
