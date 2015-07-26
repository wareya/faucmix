g++ -DNO_OPUS -std=c++11 *.cpp `sdl2-config --static-libs --cflags|sed s/-XCClinker//` -static -o player.exe \
-g -ggdb -Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors -mconsole
