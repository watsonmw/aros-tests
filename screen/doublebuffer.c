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
#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 320

typedef unsigned char u8;

/*
 * Fullscreen doublebuffer example using only AmigaOS functions.
 *
 * Build in docker:
 * docker run --rm -v /amiga:/amiga -it amigadev/crosstools:m68k-amigaos bash
 * gcc doublebuffer.c -lamiga -lm -o doublebuffer
 *
 * When running in WinUAE:
 *
 *  Disable the bottom status bar, otherwise it flickers on/off as the 'offscreen'
 *  buffer is used.  The setting is under 'Miscellaneous' -> GUI -> 'Select',
 *  change to 'Minimal'.
 */

/*
 * In C static variables are always zeroed - in contrast to other variables which get set to junk values or zero
 * depending on compiler / compiler options (debug usually zeros).
 */
static struct IntuitionBase* IntuitionBase;
static struct GfxBase* GfxBase;

static struct Screen* aosScreen;
static struct Window* aosWindow;

/* Alternating Screen buffer and off-screen buffer */
static struct ScreenBuffer* aosScreenBuffer[2];

/* Message ports AOS uses to signal to us when it's safe to use above buffers after switching them */
static struct MsgPort* aosDpDispPort;
static struct MsgPort* aosDpSafePort;

/* Empty pointer / hide pointer graphic */
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
        aosWindow = 0; /* clearing pointers / handles after free is good practice to limit use after free surprises */
    }

    if (aosScreenBuffer[0]) {
        WaitBlit(); /* FreeScreenBuffer() docs recommend this WaitBlit() for buggy graphics.library versions */
        FreeScreenBuffer(aosScreen, aosScreenBuffer[0]);
        aosScreenBuffer[0] = 0;
    }

    if (aosScreenBuffer[1]) {
        WaitBlit(); /* FreeScreenBuffer() docs recommend this WaitBlit() for buggy graphics.library versions */
        FreeScreenBuffer(aosScreen, aosScreenBuffer[1]);
        aosScreenBuffer[1] = 0;
    }

    if (aosDpDispPort) {
        DeleteMsgPort(aosDpDispPort);
        aosDpDispPort = 0;
    }

    if (aosDpSafePort) {
        DeleteMsgPort(aosDpSafePort);
        aosDpSafePort = 0;
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

    LoadRGB4(&aosScreen->ViewPort, colours, 2);

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

    SetPointer(aosWindow, nullPointerGraphic, 1, 16, 0, 0);

    aosDpDispPort = CreateMsgPort();
    aosDpSafePort = CreateMsgPort();

    aosScreenBuffer[0] = AllocScreenBuffer(aosScreen, NULL, SB_SCREEN_BITMAP);
    aosScreenBuffer[1] = AllocScreenBuffer(aosScreen, NULL, 0);

    for (int i = 0; i < 2; i++) {
        aosScreenBuffer[i]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = aosDpDispPort;
        aosScreenBuffer[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = aosDpSafePort;
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

static int AOS_processEvents() {
    struct IntuiMessage* msg;
    int close = FALSE;

    while ((msg = (struct IntuiMessage*) GetMsg(aosWindow->UserPort))) {
        switch (msg->Class) {
            case IDCMP_CLOSEWINDOW:
                close = TRUE;
                break;
            case IDCMP_RAWKEY: {
                WORD code = msg->Code & ~IECODE_UP_PREFIX;
                if (code == KC_ESC) {
                    close = TRUE;
                }
                break;
            }
            case IDCMP_MOUSEBUTTONS: {
                WORD code = msg->Code;
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
    struct RastPort rastPort;
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

    InitRastPort(&rastPort);

    while (AOS_processEvents()) {
        /* Wait for off-screen bitmap to be writable */
        if (!dbSafeToWrite) {
            while (!GetMsg(aosDpSafePort)) {
                Wait(1 << (aosDpSafePort->mp_SigBit));
            }
            dbSafeToWrite = TRUE;
        }

        rastPort.BitMap = aosScreenBuffer[dbCurBuffer]->sb_BitMap;

        AOS_clr(&rastPort);
        for (int j = 0; j < 30; j++) {
            moveInsect(&insect[j]);
            int x = insect[j].x >> 16;
            int y = insect[j].y >> 16;
            AOS_DrawPixel(&rastPort, x, y);
        }

        /* Wait for on-screen bitmap to be fully displayed */
        if (!dbSafeToChange) {
            while (!GetMsg(aosDpDispPort)) {
                Wait(1 << (aosDpDispPort->mp_SigBit));
            }
            dbSafeToChange = TRUE;
        }

        if (ChangeScreenBuffer(aosScreen, aosScreenBuffer[dbCurBuffer])) {
            dbSafeToChange = FALSE;
            dbSafeToWrite = FALSE;
            /* toggle current buffer */
            dbCurBuffer ^=1;
        }
    }

    /* cleanup for pending messages */
    if (!dbSafeToChange) {
        while (!GetMsg(aosDpDispPort)) {
            Wait(1 << (aosDpDispPort->mp_SigBit));
        }
    }

    if (!dbSafeToWrite) {
        while (!GetMsg(aosDpSafePort)) {
            Wait(1 << (aosDpSafePort->mp_SigBit));
        }
    }

    AOS_cleanupAndExit(0);

    return 0;
}
