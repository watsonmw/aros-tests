#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>

#define KC_ESC 0x45
#define SCREEN_HEIGHT 256
#define SCREEN_WIDTH 320

//
// Open a 1 bit screen in fullscreen mode to draw on, and a window to process events.
// Use the raster to draw to the screens bitmap like good amigos.
// No delay between frames - i.e. draw as fast as possible.
//

typedef unsigned char u8;

static struct IntuitionBase* IntuitionBase;
static struct GfxBase* GfxBase;

static struct Screen* aosScreen;
static struct Window* aosWindow;

static UWORD nullPointerGraphic[] = {
        0x0000, 0x0000, /* reserved, must be NULL */
        0x0000, 0x0000, /* 1 row of image data */
        0x0000, 0x0000  /* reserved, must be NULL */
};

typedef struct sInsect {
    long dx;
    long dy;
    long x;
    long y;
    unsigned char angle;
    unsigned char dangle;
    int speed;
    int c;
} Insect;

static long fcos[256];
static long fsin[256];

static short colours[2] = {
    0x0000, 0x0f0f
};

void AOS_DrawPixel(struct RastPort* rastPort, int x, int y) {
    SetAPen(rastPort, 1L);
    WritePixel(rastPort, x, y);
}

void AOS_clr(struct RastPort* rastPort) {
    SetAPen(rastPort, 0L);
    RectFill(rastPort, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
}

void AOS_cleanupAndExit(int exitCode) {
    if (aosWindow) {
        ClearPointer(aosWindow);
        CloseWindow(aosWindow);
        aosWindow = 0;
    }

    if (aosScreen) {
        CloseScreen(aosScreen);
        aosScreen = 0;
    }

    if (IntuitionBase) {
        CloseLibrary((struct Library*) IntuitionBase);
    }

    if (GfxBase) {
        CloseLibrary((struct Library*) GfxBase);
    }

    exit(exitCode);
}

void AOS_init() {
    if (!(IntuitionBase = (struct IntuitionBase*) OpenLibrary((UBYTE*) "intuition.library", 39))) {
        AOS_cleanupAndExit(0);
    }

    if (!(GfxBase = (struct GfxBase*) OpenLibrary((UBYTE*) "graphics.library", 0))) {
        AOS_cleanupAndExit(0);
    }

    aosScreen = OpenScreenTags(NULL,
                               SA_Depth, 1,
                               SA_Width, SCREEN_WIDTH,
                               SA_Height, SCREEN_HEIGHT,
                               SA_Type, CUSTOMSCREEN,
                               SA_Quiet, TRUE,
                               SA_ShowTitle, FALSE,
                               SA_Draggable, FALSE,
                               SA_Exclusive, TRUE,
                               SA_AutoScroll, FALSE,
                               TAG_END);

    if (aosScreen == NULL) {
        AOS_cleanupAndExit(0);
    }

    LoadRGB4(&aosScreen->ViewPort, colours, 2L);

    aosWindow = OpenWindowTags(NULL,
                               WA_Left, 0,
                               WA_Top, 0,
                               WA_Width, SCREEN_WIDTH,
                               WA_Height, SCREEN_HEIGHT,
                               WA_CustomScreen, aosScreen,
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

    if (!aosWindow) {
        AOS_cleanupAndExit(0);
    }
}

void moveInsect(Insect* insect) {
    if (insect->c <= 0) {
        insect->c = rand() % 10 + 5;
        insect->dangle = rand() % 10 - 5;
    }

    insect->c--;
    insect->angle += insect->dangle;
    insect->dx = insect->speed * fcos[insect->angle];
    insect->dy = insect->speed * fsin[insect->angle];
    insect->x += insect->dx;
    insect->y += insect->dy;

    if (insect->x < 0) {
        insect->x = 0;
        insect->angle = 128 - insect->angle;
        insect->c = rand() % 10 + 10;
    }

    if (insect->x >= (SCREEN_WIDTH << 16)) {
        insect->x = ((SCREEN_WIDTH - 1) << 16);
        insect->angle = 128 - insect->angle;
        insect->c = rand() % 10 + 10;
    }

    if (insect->y < 0) {
        insect->y = 0;
        insect->angle = 256 - insect->angle;
        insect->c = rand() % 10 + 10;
    }

    if (insect->y >= (SCREEN_HEIGHT << 16)) {
        insect->y = ((SCREEN_HEIGHT - 1) << 16);
        insect->angle = 256 - insect->angle;
        insect->c = rand() % 10 + 10;
    }
}

void initInsect(Insect* i) {
    i->dx = 0;
    i->dy = 0;
    i->x = (rand() % SCREEN_WIDTH << 16);
    i->y = (rand() % SCREEN_HEIGHT << 16);
    i->angle = 0;
    i->dangle = 0;
    i->speed = 3;
    i->c = 0;
}

void buildLookups() {
    int i;
    for (i = 0; i < 256; i++) {
        fsin[i] = 65536 * sin(i * M_PI * 2 / 256);
        fcos[i] = 65536 * cos(i * M_PI * 2 / 256);
    }
}

/* Process any pending events */
static int AOS_processEvents() {
    struct IntuiMessage* msg;
    int close = FALSE;

    /* Escape, left mouse and close window message exit */
    while ((msg = (struct IntuiMessage*) GetMsg(aosWindow->UserPort))) {
        switch (msg->Class) {
            case IDCMP_CLOSEWINDOW:
                // Window close button is hidden so we shouldn't get this message
                close = TRUE;
                break;
            case IDCMP_RAWKEY: {
                WORD code = msg->Code & ~IECODE_UP_PREFIX;
                // escape key exits
                if (code == KC_ESC) {
                    close = TRUE;
                }
                break;
            }
            case IDCMP_MOUSEBUTTONS: {
                WORD code = msg->Code;
                // left mouse exits
                if (code == SELECTDOWN) {
                    close = TRUE;
                }
                break;
            }
        }
        ReplyMsg((struct Message*) msg);
    }

    return !close;
}

int main(int argc, char** argv) {
    Insect insect[30];

    u8 dbSafeToChange = TRUE;
    u8 dbSafeToWrite = TRUE;
    u8 dbCurBuffer = 1;

    AOS_init();
    srand(4);

    for (int i = 0; i < 30; i++) {
        initInsect(&insect[i]);
    }

    buildLookups();

    while (AOS_processEvents()) {
        AOS_clr(&aosScreen->RastPort);
        for (int j = 0; j < 30; j++) {
            moveInsect(&insect[j]);
            int x = insect[j].x >> 16;
            int y = insect[j].y >> 16;
            AOS_DrawPixel(&aosScreen->RastPort, x, y);
        }
    }

    AOS_cleanupAndExit(0);

    return 0;
}
