#g++ -Wall -g -ggdb -std=c++11 faucetmix2.cpp -lSDL2

#!/bin/bash


mkdir -p obj
source=(
 "newmain.cpp"
 "wavfile.cpp"
 "wavstream.cpp"
 "emitter.cpp"
 "format.cpp"
 "resample.cpp"
 "stream.cpp"
 "channels.cpp"
 "api.cpp"
 "respondtoSDL.cpp"
 "global.cpp"
 )

if [ "$OSTYPE" == "msys" ]; then
    executable="player.exe"
else
    executable="player.out"
fi

if [ "$OSTYPE" == "msys" ]; then
    echo "Platform seems to be windows. Report this bug if this is not the case."
    
    forceinclude="`sdl2-config --prefix`"
    sdliflags="`sdl2-config --cflags`"
    sdllflags="`sdl2-config --static-libs` -lSDL2_image -static"
    cflags="$sdliflags -I${forceinclude}/"
    linker="-L /usr/lib -static-libstdc++ -static-libgcc $sdllflags -mconsole -mwindows"

    if hash sdl2-config; then
        cat /dev/null;
    else
        echo "Could not find sdl2-config. Is SDL2 installed correctly? Aborting."
        exit 1
    fi

    echo ""
    echo "Checking sdl2-config --prefix: ${forceinclude}"
    if [ ! -f "${forceinclude}/lib/libSDL2.a" ]; then
        echo "sdl2-config prefix does not seem to be valid: edit sdl2-config."
        echo "Aborting."
        exit 1
    fi
    echo "Looks okay."
    echo "Also, if you get an 'XCClinker' error, remove that flag from sdl2_config."
    echo ""
else
    echo "Platform seems to be linux. If not, $OSTYPE is wrong."
    
    if hash pkg-config 2>/dev/null; then # prefer pkg-config to sdl2-config
        echo "Using pkg-config for SDL2 compiler flags."
        sdliflags="`pkg-config --cflags sdl2`"
        sdllflags="`pkg-config --libs sdl2` -lSDL2_image"
        cflags="-Iinclude $sdliflags"
        linker="-L /usr/lib $sdllflags"
    else
        forceinclude="`sdl2-config --prefix`" # avoid unfortunate packing mistake
        sdliflags="`sdl2-config --cflags`"
        sdllflags="`sdl2-config --libs` -lSDL2_image"
        cflags="$sdliflags -I${forceinclude}/"
        linker="-L /usr/lib $sdllflags"
        if hash sdl2-config; then
            cat /dev/null;
        else
            echo "Could not find sdl2-config. Is SDL2 installed correctly? Aborting."
            exit 1
        fi
        
        echo ""
        echo "Checking sdl2-config --prefix: ${forceinclude}"
        if [ ! -f "${forceinclude}/lib/libSDL2.so" ]; then
            echo "sdl2-config prefix does not seem to be valid: edit sdl2-config."
            echo "Aborting."
            exit 1
        fi
        echo "Looks okay."
    fi
fi

cmd="g++ -std=c++11 -g -ggdb -Wall -pedantic -Wno-unused -Wno-sign-compare -Wno-return-type -Wfatal-errors $cflags"

objects=""

for i in "${source[@]}"
do
    obj="`echo $i | sed 's-src/-obj/-g' | sed 's-.cpp-.o-g'`"
    deps=($(gcc -std=c++11 -MM $i | sed -e 's/^\w*.o://' | tr '\n' ' ' | sed -e 's/\\//g' | sed 's/ \+//' | sed 's/ \+/\n/g'))
    for j in "${deps[@]}"
    do
        if test $j -nt $obj; then
            echo "$cmd -c $i -o $obj"
            $cmd -c $i -o $obj
			break
        fi
    done
    objects="$objects $obj"
done
echo "g++ -o $executable $objects $linker"
g++ -o $executable $objects $linker

echo "done"

