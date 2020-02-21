#!/usr/bin/env sh

#
# Examples built with gcc using Docker image 'amigadev/crosstools'
#
#   docker run --rm -v /amiga:/amiga -it amigadev/crosstools:m68k-amigaos bash
#

gcc hello/null.c -lamiga -lm -o build/null
gcc hello/hello.c -lamiga -lm -o build/hello
gcc hello/graphics.c -lamiga -lm -o build/graphics
gcc window/window.c -lamiga -lm -o build/window
gcc screen/doublebuffer.c -lamiga -lm -o build/doublebuffer
gcc screen/fullscreen.c -lamiga -lm -o build/fullscreen
