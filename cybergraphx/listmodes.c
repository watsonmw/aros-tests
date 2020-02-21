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
// - when no match returns 'null' node instead of empty list (cgx v3 41.4 returns NULL)
// - Node.ln_Name on nodes is 'null' instead of the same as CyberModeNode.ModeText
// 8bit filter:
// - doesn't filter by CYBRMREQ_CModelArray
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
        printf("list: %x %x %x\n", list->lh_Head, list->lh_Tail, list->lh_TailPred);
        while (node != NULL && node != list->lh_TailPred) {
            printf("node: %x %s %x %x\n", node, node->ln_Name, node->ln_Type, node->ln_Pred);
            struct CyberModeNode* cgxNode = (struct CyberModeNode*) node;
            printf("mode %d %d %d %d -%s-\n", cgxNode->Width, cgxNode->Height, cgxNode->Depth,
                   cgxNode->DisplayID, cgxNode->ModeText);
            node = node->ln_Succ;
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

    printf("--- List all 8bit modes using CYBRMREQ_CModelArray - instead AROS returns 1 junk / empty node\n");
    printModeList(list);

    printf("\"--- List all 8bit modes using CYBRMREQ_Min/MaxDepth - max len CyberModeNode.ModeText not null terminated\n");
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