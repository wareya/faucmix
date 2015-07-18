i686-w64-mingw32-g++ -std=c++11 `echo *.cpp|sed s/newmain.cpp//` `i686-w64-mingw32-pkg-config opusfile sdl2 --static --libs --cflags|sed s/-XCClinker//` -static -o cross.dll \
-s -g -ggdb -Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors -mconsole -shared -o fauxmix.dll
i686-w64-mingw32-ar rcs libfauxmix.a fauxmix.dll
