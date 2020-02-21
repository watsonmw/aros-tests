#include <stdio.h>
#include <stdlib.h>

#include <intuition/intuition.h>
#include <clib/intuition_protos.h>
#include <graphics/gfxbase.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>

/*
 * Load and unload intuition and graphics libraries
 */

static struct IntuitionBase* IntuitionBase;
static struct GfxBase* GfxBase;

void AOS_cleanupAndExit(int exitCode) {

    if (GfxBase) {
        printf("close graphics.library\n");
        CloseLibrary((struct Library*) GfxBase);
    }

    if (IntuitionBase) {
        printf("close intuition.library\n");
        CloseLibrary((struct Library*) IntuitionBase);
    }

    printf("end\n");

    exit(exitCode);
}

int main(int argc, char** argv) {
    printf("start\n");
    if (!(IntuitionBase = (struct IntuitionBase*) OpenLibrary((UBYTE*) "intuition.library", 39))) {
        AOS_cleanupAndExit(0);
    }

    printf("loaded intuition.library\n");

    if (!(GfxBase = (struct GfxBase*) OpenLibrary((UBYTE*) "graphics.library", 0))) {
        AOS_cleanupAndExit(0);
    }

    printf("loaded graphics.library\n");

    AOS_cleanupAndExit(0);

    return 0;
}
