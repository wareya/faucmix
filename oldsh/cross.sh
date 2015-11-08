i686-w64-mingw32-g++ -std=c++11 *.cpp `i686-w64-mingw32-pkg-config opusfile sdl2 --static --libs --cflags|sed s/-XCClinker//` -static -o crossplayer.exe \
-g -ggdb -Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors -mconsole
