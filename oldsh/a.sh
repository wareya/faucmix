cat `echo *.cpp|sed s/newmain.cpp//` | g++ -fPIC -std=c++11 -x c++ -c - `pkg-config opusfile sdl2 --static --libs --cflags|sed s/-XCClinker//` \
-s -g -ggdb -Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors -shared -o fauxmix.so
#ar rvs libfauxmix.a fauxmix.o
