#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>

#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>

static struct IntuitionBase* IntuitionBase;
static struct GfxBase* GfxBase;
static struct Library *CyberGfxBase;

// AROS issues:
//
// - CyberModeNode.ModeText is not null terminated
// - doesn't filter by CYBRMREQ_CModelArray, although min and max depth work
//

void AOS_cleanupAndExit(int exitCode) {
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

void printModeList(struct List* list) {
    if (list) {
        struct Node* node = list->lh_Head;
        printf("list: %x %x %x empty:%d\n", list->lh_Head, list->lh_Tail, list->lh_TailPred, IsListEmpty(list));
        for (; node->ln_Succ != NULL; node = node->ln_Succ) {
            printf("node: %x %s %x %x\n", node, node->ln_Name, node->ln_Type, node->ln_Pred);
            struct CyberModeNode* cgxNode = (struct CyberModeNode*) node;
            printf("mode %d %d %d %d -%s-\n", cgxNode->Width, cgxNode->Height, cgxNode->Depth,
                   cgxNode->DisplayID, cgxNode->ModeText);
        }
        FreeCModeList(list);
    }
}

void AOS_init() {
    if (!(IntuitionBase = (struct IntuitionBase*) OpenLibrary((UBYTE*) "intuition.library", 39))) {
        AOS_cleanupAndExit(0);
    }

    if (!(GfxBase = (struct GfxBase*) OpenLibrary((UBYTE*) "graphics.library", 0))) {
        AOS_cleanupAndExit(0);
    }

    if (!(CyberGfxBase = OpenLibrary("cybergraphics.library", 41))) {
        AOS_cleanupAndExit(0);
    }

    UWORD ModelArrayLUT8[] = {PIXFMT_LUT8, ~0};
    struct List* list = AllocCModeListTags(CYBRMREQ_CModelArray, (ULONG) ModelArrayLUT8, TAG_END);

    printf("--- List all 8bit modes using CYBRMREQ_CModelArray - AROS returns empty / head node and no 8bit modes\n");
    printModeList(list);

    printf("\"--- List all 8bit modes using CYBRMREQ_Min/MaxDepth - AROS returns 8bit modes, but CyberModeNode.ModeText not null terminated / contains extra chars\n");
    list = AllocCModeListTags(CYBRMREQ_MinDepth, 8,
                              CYBRMREQ_MaxDepth, 8,
                              TAG_END);

    printModeList(list);
}

int main(int argc, char** argv) {
    AOS_init();
    AOS_cleanupAndExit(0);
    return 0;
}
