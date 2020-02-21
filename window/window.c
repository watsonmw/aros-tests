#include <stdio.h>
#include <stdlib.h>

#include <intuition/intuition.h>
#include <clib/intuition_protos.h>
#include <graphics/gfxbase.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>

/*
 * Open and immediately close a window on current screen
 */

static struct IntuitionBase* IntuitionBase;
static struct GfxBase* GfxBase;
static struct Window* aosWindow;

void AOS_cleanupAndExit(int exitCode) {

    if (GfxBase) {
        printf("close graphics.library\n");
        CloseLibrary((struct Library*) GfxBase);
    }

    if (IntuitionBase) {
        printf("close intuition.library\n");
        CloseLibrary((struct Library*) IntuitionBase);
    }

    if (aosWindow) {
        printf("close window\n");
        CloseWindow(aosWindow);
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

    printf("open window\n");

    aosWindow = OpenWindowTags(NULL,
                               WA_Left, 0,
                               WA_Top, 0,
                               WA_Width, 200,
                               WA_Height, 200,
                               WA_Title, NULL,
                               WA_Backdrop, TRUE,
                               WA_Borderless, TRUE,
                               WA_DragBar, FALSE,
                               WA_Activate, TRUE,
                               WA_SmartRefresh, TRUE,
                               WA_NoCareRefresh, TRUE,
                               WA_Activate, TRUE,
                               WA_RMBTrap, TRUE,
                               WA_ReportMouse, TRUE,
                               WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_ACTIVEWINDOW,
                               TAG_DONE);

    AOS_cleanupAndExit(0);

    return 0;
}
