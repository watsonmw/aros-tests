#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <libraries/asl.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>
#include <clib/asl_protos.h>

#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>

#define KC_ESC 0x45

typedef unsigned char u8;

/*
 * Fullscreen doublebuffer 8bit LUT Cybergraphx example.
 *
 * Uses the AmigaOS recommended way of double buffering the screen.
 *
 * Writes directly to screen bitmap locking and unlocking as needed, as recommended in Cybergraphx docs.
 *
 * Works in UAE with 3.1.4 and RTG enabled.
 *
 * Doesn't currently (24/3/2020) work with AROS.
 */

static struct IntuitionBase* IntuitionBase;
static struct GfxBase* GfxBase;
static struct Library* CyberGfxBase;
static struct Library* AslBase;

static struct Screen* aosScreen;
static struct Window* aosWindow;
static struct ScreenBuffer* aosScreenBuffer[2];
static struct MsgPort* aosDpDispPort;
static struct MsgPort* aosDpSafePort;

static int screenWidth = 0;
static int screenHeight = 0;

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

static short colours[16] = {
    0x0000, 0x0f0f
};

void AOS_cleanupAndExit(int exitCode) {
    if (aosWindow) {
        CloseWindow(aosWindow);
        ClearPointer(aosWindow);
        aosWindow = 0;
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
    }

    if (aosDpSafePort) {
        DeleteMsgPort(aosDpSafePort);
    }

    if (aosScreen) {
        CloseScreen(aosScreen);
        aosScreen = 0;
    }

    if (AslBase) {
        CloseLibrary(AslBase);
    }

    if (CyberGfxBase) {
        CloseLibrary(CyberGfxBase);
    }

    if (GfxBase) {
        CloseLibrary((struct Library*) GfxBase);
    }

    if (IntuitionBase) {
        CloseLibrary((struct Library*) IntuitionBase);
    }

    exit(exitCode);
}

// GCC hooks handling - other compilers require different syntax see hooks.h
ULONG Hook_OnlyRTGModes(register struct Hook* hook __asm("a0"),
                        register struct ScreenModeRequester* smr __asm("a2"),
                        register ULONG displayModeId __asm("a1"))
{
    return IsCyberModeID(displayModeId) && GetCyberIDAttr(CYBRIDATTR_DEPTH, displayModeId) == 8;
}

void AOS_init() {
    if (!(IntuitionBase = (struct IntuitionBase*) OpenLibrary((UBYTE*) "intuition.library", 39))) {
        AOS_cleanupAndExit(0);
    }

    if (!(GfxBase = (struct GfxBase*) OpenLibrary((UBYTE*) "graphics.library", 0))) {
        AOS_cleanupAndExit(0);
    }

    if (!(AslBase = OpenLibrary((UBYTE*) "asl.library", 38))) {
        AOS_cleanupAndExit(0);
    }

    if (!(CyberGfxBase = OpenLibrary("cybergraphics.library",41))) {
        AOS_cleanupAndExit(0);
    }

    ULONG modeId = INVALID_ID;

    struct Hook screenModeFilterHook;
    screenModeFilterHook.h_Entry = (HOOKFUNC)Hook_OnlyRTGModes;
    screenModeFilterHook.h_SubEntry = NULL;
    screenModeFilterHook.h_Data = 0;

    struct ScreenModeRequester *smr = (struct ScreenModeRequester*)
            AllocAslRequestTags(ASL_ScreenModeRequest,
                                ASLSM_TitleText, "Select Screen Res",
                                ASLSM_MinDepth, 8,
                                ASLSM_MaxDepth, 8,
                                ASLSM_FilterFunc, (ULONG) &screenModeFilterHook,
                                TAG_END);


    if (smr) {
        if (AslRequest(smr, 0L)) {
            modeId = smr->sm_DisplayID;
        }

        FreeAslRequest(smr);
    }

    screenWidth = GetCyberIDAttr(CYBRIDATTR_WIDTH, modeId);
    screenHeight = GetCyberIDAttr(CYBRIDATTR_HEIGHT, modeId);

    if (modeId == INVALID_ID) {
        AOS_cleanupAndExit(0);
    }

    aosScreen = OpenScreenTags(NULL,
                               SA_Depth, 8,
                               SA_DisplayID, modeId,
                               SA_Width, screenWidth,
                               SA_Height, screenHeight,
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
                               WA_Width, screenWidth,
                               WA_Height, screenHeight,
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

    if (insect->x >= (screenWidth << 16)) {
        insect->x = ((screenWidth - 1) << 16);
        insect->angle = 128 - insect->angle;
        insect->c = rand() % 10 + 10;
    }

    if (insect->y < 0) {
        insect->y = 0;
        insect->angle = 256 - insect->angle;
        insect->c = rand() % 10 + 10;
    }

    if (insect->y >= (screenHeight << 16)) {
        insect->y = ((screenHeight - 1) << 16);
        insect->angle = 256 - insect->angle;
        insect->c = rand() % 10 + 10;
    }
}

void initInsect(Insect* i) {
    i->dx = 0;
    i->dy = 0;
    i->x = (rand() % screenWidth << 16);
    i->y = (rand() % screenHeight << 16);
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
        if (!dbSafeToWrite) {
            while (!GetMsg(aosDpSafePort)) {
                Wait(1 << (aosDpSafePort->mp_SigBit));
            }
            dbSafeToWrite = TRUE;
        }

        u8* buffer = NULL;
        ULONG bytesPerRow = 0;
        ULONG pixelFormat = 0;

        APTR handle = LockBitMapTags(aosScreenBuffer[dbCurBuffer]->sb_BitMap,
                LBMI_BASEADDRESS, (ULONG)&buffer,
                LBMI_BYTESPERROW, (ULONG)&bytesPerRow,
                LBMI_PIXFMT, (ULONG)&pixelFormat,
                TAG_DONE);

        if (handle && buffer) {
            if (pixelFormat != PIXFMT_LUT8) {
                UnLockBitMap(handle);
                printf("Pixel format not supported: %d\n", pixelFormat);
                AOS_cleanupAndExit(0);
            }

            memset(buffer, 0, bytesPerRow * screenHeight);
            for (int j = 0; j < 30; j++) {
                moveInsect(&insect[j]);
                int x = insect[j].x >> 16;
                int y = insect[j].y >> 16;
                buffer[x + (y * bytesPerRow)] = 1;
            }
            UnLockBitMap(handle);
        }

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
            dbCurBuffer ^= 1;
        } else {
            printf("Change screen buff failed\n");
        }
    }

    /* cleanup pending messages */
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
