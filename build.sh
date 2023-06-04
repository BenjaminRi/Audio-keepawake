#!/bin/sh

#gcc -I/usr/include/SDL2 main.c -lSDL2_mixer -lSDL2
#gcc -I/usr/include/SDL2 main.c -lSDL2_mixer -lSDL2 -fsanitize=address -static-libasan -g
#gcc -I/usr/include/SDL2 main.c -lSDL2_mixer -lSDL2 -fsanitize=thread -static-libtsan -g

OUTFILE="./build/audio-keepawake"

gcc -I/usr/include/SDL2 src/main.c -lSDL2_mixer -lSDL2 -fsanitize=address -static-libasan -g -o "$OUTFILE"
