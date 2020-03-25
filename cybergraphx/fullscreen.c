#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <libraries/asl.h>
#include <devices/timer.h>

#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>
#include <clib/asl_protos.h>
#include <clib/dos_protos.h>
#include <clib/timer_protos.h>

#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>

#define KC_ESC 0x45

typedef unsigned char u8;
typedef short u16;
typedef unsigned long long u64;

/*
 * Fullscreen 8bit LUT Cybergraphx example.
 *
 * Writes directly to screen bitmap locking and unlocking as needed, as recommended in Cybergraphx docs.
 *
 * Draws moving vertical lines so you can see any screen tearing or jank.
 *
 * Works in UAE with:
 * - 3.1 with RTG enabled
 * - AROS
 */

static struct IntuitionBase* IntuitionBase;
static struct GfxBase* GfxBase;
static struct Library* CyberGfxBase;
static struct Library* AslBase;
static struct IORequest TimerDevice;
struct Device* TimerBase; // exported so AOS timer func stubs can use

static struct Screen* aosScreen;
static struct Window* aosWindow;

static int screenWidth = 0;
static int screenHeight = 0;

static UWORD MouseCursor_NullGraphic[] = {
        0x0000, 0x0000, // reserved, must be NULL
        0x0000, 0x0000, // 1 row of image data
        0x0000, 0x0000  // reserved, must be NULL
};

static u16 PaletteColours[3] = {
        0x0000, // background
        0x0fff, // bars
        0x04f4, // text
};

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

    if (TimerDevice.io_Device) {
        CloseDevice(&TimerDevice);
    }

    exit(exitCode);
}

// GCC Hooks handling - other compilers require different syntax see hooks.h
ULONG Hook_OnlyRTGModes(register struct Hook* hook __asm("a0"),
                        register struct ScreenModeRequester* smr __asm("a2"),
                        register ULONG displayModeId __asm("a1")) {
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

    if (!(CyberGfxBase = OpenLibrary("cybergraphics.library", 41))) {
        AOS_cleanupAndExit(0);
    }

    ULONG modeId = INVALID_ID;

    struct Hook screenModeFilterHook;
    screenModeFilterHook.h_Entry = (HOOKFUNC) Hook_OnlyRTGModes;
    screenModeFilterHook.h_SubEntry = NULL;
    screenModeFilterHook.h_Data = 0;

    struct ScreenModeRequester* smr = (struct ScreenModeRequester*)
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

    LoadRGB4(&aosScreen->ViewPort, PaletteColours, 3);

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

    // Empty pointer
    SetPointer(aosWindow, MouseCursor_NullGraphic, 1, 16, 0, 0);

    OpenDevice((CONST_STRPTR)"timer.device", 0, &TimerDevice, 0);
    TimerBase = TimerDevice.io_Device;
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

u64 AOS_GetClockCount() {
    struct EClockVal clock;
    ReadEClock(&clock);
    return (((u64) clock.ev_hi) << 32u) | clock.ev_lo;
}

ULONG AOS_GetClockCountAndInterval(ULONG* tickInterval) {
    struct EClockVal clock;
    *tickInterval = ReadEClock(&clock);
    return (((u64) clock.ev_hi) << 32u) | clock.ev_lo;
}

int main(int argc, char** argv) {
    AOS_init();
    struct RastPort* rastPort = &aosScreen->RastPort;

    int verticalLineX = 0;
    int lineSpeed = screenWidth / 100;

    int frames = 0;
    int fps = 0;
    ULONG updateFpsTimer = 0;
    ULONG tickInterval = 0;
    char frameRateString[32];

    u64 prevClock = AOS_GetClockCountAndInterval(&tickInterval);

    while (AOS_processEvents()) {
        u8* buffer = NULL;
        ULONG bytesPerRow = 0;
        ULONG pixelFormat = 0;

        WaitTOF();

        APTR handle = LockBitMapTags(rastPort->BitMap,
                                     LBMI_BASEADDRESS, (ULONG) &buffer,
                                     LBMI_BYTESPERROW, (ULONG) &bytesPerRow,
                                     LBMI_PIXFMT, (ULONG) &pixelFormat,
                                     TAG_DONE);
        verticalLineX += lineSpeed;
        if (verticalLineX >= screenWidth - 16) {
            verticalLineX = screenWidth - 17;
            lineSpeed = -lineSpeed;
        }

        if (verticalLineX < 0) {
            verticalLineX = 0;
            lineSpeed = -lineSpeed;
        }

        if (handle && buffer) {
            if (pixelFormat != PIXFMT_LUT8) {
                UnLockBitMap(handle);
                printf("Pixel format not supported: %d\n", pixelFormat);
                AOS_cleanupAndExit(0);
            }

            u8* bufferLine = buffer;
            for (int i = 0; i < screenHeight; i++) {
                int j = 0;
                int end = verticalLineX;
                for (; j < end && j < screenWidth; j++) {
                    bufferLine[j] = 0;
                }
                end += 4;
                for (; j < end && j < screenWidth; j++) {
                    bufferLine[j] = 1;
                }
                end += 8;
                for (; j < end && j < screenWidth; j++) {
                    bufferLine[j] = 0;
                }
                end += 4;
                for (; j < end && j < screenWidth; j++) {
                    bufferLine[j] = 1;
                }
                for (; j < screenWidth; j++) {
                    bufferLine[j] = 0;
                }

                bufferLine += bytesPerRow;
            }

            UnLockBitMap(handle);
        }

        frames++;

        u64 currentClock = AOS_GetClockCount();
        ULONG elapsed = ((ULONG)(currentClock - prevClock)) / (tickInterval / 1000);
        prevClock = currentClock;
        updateFpsTimer += elapsed;
        if (updateFpsTimer > 1000) {
            fps = frames - 1;
            frames = 0;
            updateFpsTimer = updateFpsTimer - 1000;
        }

        if (fps > 0) {
            snprintf(frameRateString, 32, "%ld fps", fps);

            SetAPen(rastPort, 2);
            SetBPen(rastPort, 0);
            Move(rastPort, 10, 10);
            Text(rastPort, (CONST_STRPTR)frameRateString, strlen(frameRateString));
        }
    }

    AOS_cleanupAndExit(0);

    return 0;
}
