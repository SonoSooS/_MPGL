#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "config.h"

#include "GL/ctor.h"
#include "GL/core.h"
#include "GL/utility.h"

#include "player/mmplayer.h"

#include "render.h"
#include "render_grh.h"
#include "render_gr.h"
#include "render_grf.h"



#ifdef PIANOKEYS
const DWORD keymul = 8;
#else
const DWORD keymul = 1;
#endif

#ifdef TIMI_CAPTURE
const ULONGLONG FPS_DENIM = TIMI_FPS_DENIM;//625*4;//28125;//3840;
const ULONGLONG FPS_NOMIN = TIMI_FPS_NOMIN;//100;//130;
ULONGLONG FPS_frame = 0;
volatile BOOL FPS_capture = FALSE;

#ifdef TIMI_CUSTOMSCROLL
ULONGLONG FPS_scroll = 0;
#endif
#endif


const float minheight = (1.0F / 256.0F)
#ifdef KEYBOARD
 * 1.5
#endif
#ifdef TRANSFORM
/ 2
#endif
;

#if defined(HDR) && !defined(FASTHDR)
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
#endif

#if defined(ROUNDEDGE) && !defined(DYNASCROLL)
//#define DYNASCROLL
#endif

#if defined(ROUNDEDGE) && defined(GLTEXT)
#error Text rendering is not supported with round edges
#endif

#if !defined(GLTEXT) && (defined(TEXTNPS))
#error This feature combination requires GLTEXT
#endif

#ifdef DYNASCROLL

#define TICKVAL syncvalue
#define TICKVAR mmnotesync

#ifdef TRANSFORM
#define tickheight 100000
#else
#define tickheight 40000
#endif

const float tickscale = 1.0F / (float)tickheight;
const DWORD minwidth = tickheight / 32;

#else
const DWORD minwidth = 16;
#ifdef TIMI_CUSTOMSCROLL
#define TICKVAL FPS_scroll
#else
#define TICKVAL player->TickCounter
#endif
#define TICKVAR ply->TickCounter
#endif



extern HWND glwnd;
extern HDC dc;
extern HGLRC glctx;
extern HANDLE vsyncevent;
extern RECT erect;

extern BOOL canrender;

extern HMODULE KSModule;


extern QWORD midisize;


#ifdef TEXTNPS
struct histogram* hist;
ULONGLONG notecounter;
#endif



ULONGLONG drawnotesraw = 0;
ULONGLONG drawnotes = 0;

void WINAPI DebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if(type == GL_DEBUG_TYPE_ERROR)
    printf("GL ERROR %i | source = 0x%X, type = 0x%X, severity = 0x%X, message = %s\n",
        id, source, type, severity, message);
}


typedef struct NoteNode
{
    struct NoteNode* next;
    DWORD uid;
    ULONGLONG start;
    ULONGLONG end;
#ifdef BUGFIXTEST
    struct NoteNode* overlapped_voce;
    size_t junk;
#endif
} NoteNode;

MMPlayer* ply;
MMPlayer* player;

struct quad* quads;
size_t vtxidx;
const size_t vertexsize =
#ifdef _M_IX86
    1 << 12;
#else
    //1 << 18;
    1 << 12;
#endif

/*
main
- NoteFree
  - freelist_head = node
  - freelist = node->next = freelist

- VisibleNoteList = 0
- VisibleNoteListHead = 0

- freelist_return
- freelist_returnhead = freelist_head
- freelist_return = freelist
- freelist_head = 0
- freelist = 0

notecatcher
- NoteAppend
  - VisibleNoteListHead->next
  - VisibleNoteListHead = node
  
  - VisibleNoteList = node

- NoteAlloc
  - return notelist
  - notelist = notelist->next

- NoteReturn
  - freelist_return
  - freelist_returnhead
  - freelist_returnhead->next = notelist
  - notelist = freelist_return
  - freelist_returnhead = 0
  - freelist_return = 0
*/

static NoteNode* notelist;
static NoteNode* freelist;
static NoteNode* freelist_head;
static NoteNode* freelist_return;
static NoteNode* freelist_returnhead;

static NoteNode* VisibleNoteList;
static NoteNode* VisibleNoteListHead;

static NoteNode* *ActiveNoteList;
static KCOLOR*   colortable;

static QWORD notealloccount;
static QWORD currnotealloc;


//seems to work properly
static inline void NoteAppend(NoteNode* __restrict node)
{
    //printf("NoteAppend %08X %lli\n", node->uid, node->start);
    
    node->next = 0; //we append at the end, so the next ptr must always be null to signal list end
    
    if(VisibleNoteList) //VisibleNoteListHead is guaranteed to be set to a valid value
    {
        //put note at the end of the list
        VisibleNoteListHead->next = node;
        //move list end forward
        VisibleNoteListHead = node;
    }
    else
    {
        //set head first
        VisibleNoteListHead = node;
        //then set the first element
        VisibleNoteList = node;
    }
}

static inline NoteNode* __restrict NoteAlloc()
{
    if(notelist)
    {
        //pop from list
        
        NoteNode* __restrict ret = notelist; //pop last
        notelist = ret->next; //set list tail to next
        //ret->next = 0; //no need to set next to 0 because it'll be inserted into a list anyways which'll set this value
        return ret;
    }
    
    NoteNode* __restrict rn = (NoteNode*)malloc(sizeof(NoteNode));
    
    if(rn)
        notealloccount++;
    else
        puts("Out of Memory");
    
    return rn;
}

static inline void NoteFree(NoteNode* __restrict node)
{
    if(!freelist_head) //keep track of first element for O(1) return
    {
        //if(freelist)
        //    puts("Oh no, returnhead it set after freelist has been set");
        freelist_head = node; //TODO: let's hope freelist will always be null :/
    }
    
    node->next = freelist;
    freelist = node;
}

#ifdef DYNASCROLL
static QWORD syncvalue;

static void NoteSync(MMPlayer* syncplayer, DWORD dwDelta)
{
    if(dwDelta)
    {
        DWORD dwValue = 60000000 / syncplayer->tempo;
        //while(dwDelta--)
            syncvalue += (ULONGLONG)dwValue * (ULONGLONG)dwDelta;
    }
}
#endif

#ifdef TEXTNPS

struct histogram *hist = 0;
DWORD histlast = 0;
DWORD histwrite = 0;
ULONGLONG histsum = 0;
ULONGLONG histdelay = 0;
ULONGLONG onehztimer = 0;
ULONGLONG currnote = 0;
ULONGLONG currnps = 0;

int(WINAPI*kNPOriginal)(DWORD msg);
void(WINAPI*kNSOriginal)(MMPlayer* syncplayer, DWORD dwDelta);
static int WINAPI kNPIntercept(DWORD note)
{
    if(((BYTE)note & 0xF0) == 0x90 && (BYTE)(note >> 8))
        currnote++;
    
    if(kNPOriginal)
        return kNPOriginal(note);
    else
        return 0;
}

static void kNPSync(MMPlayer* syncplayer, DWORD dwDelta)
{
    struct histogram* __restrict hhist = hist + (histwrite++);
    if(histwrite == (DWORD)1e7)
        histwrite = 0;
    
    hhist->delta = dwDelta * syncplayer->tempomulti;
    hhist->count = currnote;
    
    currnps += currnote;
    notecounter += currnote;
    
    struct histogram* __restrict ihist;
    
    while(onehztimer > (DWORD)1e7)
    {
        ihist = hist + (histlast++);
        if(histlast == (DWORD)1e7)
            histlast = 0;
        
        onehztimer -= ihist->delta;
        currnps -= ihist->count;
    }
    
    onehztimer += hhist->delta;
    
    currnote = 0;
    
    /*
    #ifdef DYNASCROLL
    return NoteSync(syncplayer, dwDelta);
    #endif
    */
    
    if(kNSOriginal)
        kNSOriginal(syncplayer, dwDelta);
}
#endif

static DWORD returnfailcount;

#ifdef DYNASCROLL
static QWORD mmnotesync;
#endif

static void NoteReturn(MMPlayer* syncplayer, DWORD dwDelta)
{
    #ifdef DYNASCROLL
    if(dwDelta)
    {
        DWORD dwValue = 60000000 / syncplayer->tempo;
        while(dwDelta--)
            mmnotesync += dwValue;
    }
    #endif
    
    if(!freelist_return)
        return;
    
    if(!freelist_returnhead)
    {
        printf("Returnhead is null, yet freelist_return is %016llX\n", freelist_return);
    }
    
    NoteNode* __restrict head = freelist_returnhead;
    freelist_returnhead = 0;
    NoteNode* f = freelist_return;
    freelist_return = 0;
    
    //append the current notelist to the end of the returnlist
    head->next = notelist;
    //set the current notelist to the returnlist head
    notelist = f;
}

__attribute__((noinline)) static void FlushToilet()
{
    if(vtxidx)
    {
        #ifdef GLTEXT
        drawnotesraw += vtxidx;
        #endif
        glBufferSubData(GL_ARRAY_BUFFER, 0, vtxidx * sizeof(*quads), quads);
        glDrawElements(GL_TRIANGLES, vtxidx * NOTEVTX, GL_UNSIGNED_INT, 0);
        vtxidx = 0;
    }
}

static int WINAPI dwLongMsg(DWORD note, LPCVOID ptr, DWORD len)
{
    if((BYTE)note != 0xF0 || !KSModule)
        return 0;
    if(len > 256)
    {
        printf("LongMsg too long (%i), ignoring\n", len);
        return 1;
    }
    
    BYTE buf[256];
    buf[0] = 0xF0;
    CopyMemory(buf + 1, ptr, len);
    
    int(WINAPI*KModMsg)(UINT,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR) = (void*)GetProcAddress(KSModule, "modMessage");
    if(KModMsg)
    {
        MIDIHDR hdr;
        ZeroMemory(&hdr, sizeof(hdr));
        hdr.dwFlags = MHDR_PREPARED;
        hdr.dwBufferLength = len + 1;
        hdr.dwBytesRecorded = len + 1;
        hdr.lpData = (LPVOID)buf;
        while(KModMsg(0, 8, 0, (DWORD_PTR)&hdr, sizeof(hdr)) == 67)
            /* do nothing */;
    }
    
    return 0;
}

#ifdef TIMI_TIEMR
static int timiTimerFunc(PULONGLONG timeout)
{
    *timeout = player->RealTime;
    return 0;
}
#endif

#ifdef TIMI_CAPTURE
static int WINAPI FPS_ShortMsg(DWORD note)
{
    return 0;
}

#ifdef TIMI_IMPRECISE
#define NEXTFRAME ((10000000ULL / FPS_DENIM) * player->timediv * FPS_frame * FPS_NOMIN)
#else
#define NEXTFRAME ((10000000ULL * FPS_frame * FPS_NOMIN * player->timediv) / FPS_DENIM)
#endif


static void FPS_SyncFunc(MMPlayer* syncplayer, DWORD dwDelta)
{
    ULONGLONG nextframe = NEXTFRAME;
    if(nextframe <= player->RealTimeUndiv)
    {
        #ifdef TIMI_CUSTOMSCROLL
        FPS_scroll = nextframe;
        #endif
        
        int(WINAPI*NtDelayExecution)(BOOL doalert, INT64* timeptr)
                = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtDelayExecution");
        
        while(!ply->done && (INT64)(ply->RealTime - (player->RealTime + ply->SyncOffset)) < 0)
        {
            INT64 sleep = -1;
            NtDelayExecution(TRUE, &sleep);
        }
        
        FPS_capture = TRUE;
        
        do
        {
            INT64 sleep = -1;
            NtDelayExecution(TRUE, &sleep);
        }
        while(FPS_capture);
        
        
        FPS_frame++;
        
        for(;;)
        {
            nextframe = NEXTFRAME;
            if(nextframe <= player->RealTimeUndiv)
            {
                puts("Frame drop");
                
                FPS_capture = TRUE;
                
                do
                {
                    INT64 sleep = -1;
                    NtDelayExecution(TRUE, &sleep);
                }
                while(FPS_capture);
                
                #ifdef TIMI_CUSTOMSCROLL
                FPS_scroll = nextframe;
                #endif
                
                FPS_frame++;
                
            }
            else
                break;
        }
    }
}
#endif

/*
    LongMessage callback for the player
    
    do stuff here idk
*/
static int WINAPI LongMessage(DWORD note, LPCVOID ptr, DWORD len)
{
    return 0;
}

/*
    Callback for the player
    
    Make sure to keep this function lower cycle count than
    KShortMsg, otherwise the note catcher will severely lag behind.
    
    Currently it's slighly less than KShortMsg, but if
    the malloc hits then it'll be much, much slower.
*/
static int WINAPI dwEventCallback(DWORD note)
{
    if((BYTE)note >= 0xA0)
        return 0;
    
    DWORD uid = (BYTE)(note >> 8) | ((note & 0xF) << 8)
    #ifdef TRACKID
    | (ply->CurrentTrack->trackid << 12)
    #endif
    ;
    
    ULONGLONG curr = TICKVAR;
    
    NoteNode* __restrict node = ActiveNoteList[uid];
    
    if((note & 0x10) && ((note >> 16) & 0xFF))
    {
        #ifndef BUGFIXTEST
        if(node)
        {
            //filter out useless notespam
            if(node->start == curr)
                return 0;
            
            //don't move note end if it has been previously set by notespam
            if(!~node->end)
            {
                #ifdef ROUNDEDGE
                    if((curr - node->start) > minwidth)
                        node->end = curr;
                    else
                        node->end = node->start + minwidth;
                #else
                    node->end = curr;
                #endif
            }
            //ActiveNoteList[uid] = 0; //already set below
        }
        #endif
        
        #ifdef BUGFIXTEST
        NoteNode* __restrict backupnode = node;
        
        node = NoteAlloc();
        
        if(!node) //allocation failed
            return 1;
        
        node->overlapped_voce = backupnode;
        
        ActiveNoteList[uid] = node;
        
        #else
        node = NoteAlloc();
        ActiveNoteList[uid] = node; //reset it no matter what
        
        if(!node) //allocation failed
            return 1;
        #endif
        
        node->start = curr;
        node->end = ~0;
        node->uid = uid
    #ifndef TRACKID
        | (ply->CurrentTrack->trackid << 12)
    #endif
        ;
        
        /*
        #ifdef BUGFIXTEST
        node->overlapped_voce = 0;
        node->junk = 0;
        #endif
        */
        
        NoteAppend(node);
        
        return 0;
    }
    
    if(node)
    {
    #ifdef BUGFIXTEST
        if(!~node->end)
            node->end = curr;
        
        ActiveNoteList[uid] = node->overlapped_voce;
    #else
        if(node->start == curr)
        {
            //end has already been set
            if(~node->end)
                return 0;
        }
        else ActiveNoteList[uid] = 0;
        
        #ifdef ROUNDEDGE
            if((curr - node->start) > minwidth)
                node->end = curr;
            else
                node->end = node->start + minwidth;
        #else
            node->end = curr;
        #endif
    #endif
    }
    
    return 0;
}

#ifndef OLDDENSE
static __attribute__((noinline)) void AddRawVtx(float offsy, float offst, float offsx, float offsr, KCOLOR color1)
{
    if(__builtin_expect(vtxidx == vertexsize, 0))
    {
        FlushToilet();
        //vtxidx = 0;
    }
    
    //offsx -= 0.125F;
    
    struct quad* __restrict ck = quads + (vtxidx++);
    
    #ifdef ROUNDEDGE
    //DWORD color = colortable[dwUID];
    //DWORD color1 = color; //(color & 0xFEFEFEFE) >> 1 | (1 << 31);
    #ifdef PFAKEY
        #ifndef HDR
        KCOLOR color2 = ((color1 & 0xFEFEFEFE) >> 1) | (0xFF << 24);
        #else
        KCOLOR color2 = (KCOLOR){
            color1.r * 0.5F,
            color1.g * 0.5F,
            color1.b * 0.5F,
            1.0F
        };
        #endif
    #else
        KCOLOR color2 = color1;
    #endif
    #ifndef HDR
    KCOLOR color3 = ((color1 & 0xFCFCFCFC) >> 2) | (0xFF << 24);
    #else
    KCOLOR color3 = (KCOLOR){
        color1.r * 0.25F,
        color1.g * 0.25F,
        color1.b * 0.25F,
        1.0F
    };
    #endif
    KCOLOR color = color1;
    //KCOLOR color2 = color1;
    //KCOLOR color3 = color;
    
    float middx = offsx + ((offsr - offsx) * 0.75F);
    float middy = offsy + ((offst - offsy) * 0.75F);
    
    /*
        0 - 1 - 2
        | /   \ |
        3 - 8 - 4
        | \   / |
        5 - 6 - 7
    */
    
    ck->quads[0] = (struct quadpart){offsx, offst, color1};
    ck->quads[1] = (struct quadpart){middx, offst, color2};
    ck->quads[2] = (struct quadpart){offsr, offst, color1};
    
    ck->quads[3] = (struct quadpart){offsx, middy, color1};
    ck->quads[4] = (struct quadpart){offsr, middy, color1};
    
    ck->quads[5] = (struct quadpart){offsx, offsy, color1};
    ck->quads[6] = (struct quadpart){middx, offsy, color2};
    ck->quads[7] = (struct quadpart){offsr, offsy, color1};
    
    ck->quads[8] = (struct quadpart){middx, middy, color3};
    
    /*float fquad = offsy + ((offst - offsy) * 0.75F);
    float lquad = offsy + ((offst - offsy) * 0.25F);
    
    ck->quads[8] = (struct quadpart){offsx, lquad, color1};
    ck->quads[9] = (struct quadpart){offsx, fquad, color1};
    
    ck->quads[0xA] = (struct quadpart){offsr, lquad, color3};
    ck->quads[0xB] = (struct quadpart){offsr, fquad, color3};*/
    
    #else
    
    //KCOLOR color1 = colortable[dwUID];
    
    #ifdef OUTLINE
    
    #ifdef PFAKEY
        #ifndef HDR
        KCOLOR color2 = ((color1 & 0xFEFEFEFE) >> 1) | (0xFF << 24);
        #else
        KCOLOR color2 = (KCOLOR){
            color1.r * 0.5F,
            color1.g * 0.5F,
            color1.b * 0.5F,
            1.0F
        };
        #endif
    #else
        KCOLOR color2 = color1;
    #endif
    
    #ifndef HDR
    KCOLOR color3 = ((color1 & 0xFCFCFCFC) >> 2) | (0xFF << 24);
    #else
    KCOLOR color3 = (KCOLOR){
        color1.r * 0.25F,
        color1.g * 0.25F,
        color1.b * 0.25F,
        1.0F
    };
    #endif
    
    float origoffsy = offsy;
    float origoffst = offst;
    
    offsy += minheight;
    offst -= minheight;
    if(offst < offsy)
    {
        ck->quads[0] = (struct quadpart){offsx, origoffst, color1};
        ck->quads[1] = (struct quadpart){offsx, origoffsy, color1};
        ck->quads[2] = (struct quadpart){offsr, origoffsy, color1};
        ck->quads[3] = (struct quadpart){offsr, origoffst, color1};
    }
    else
    {
        ck->quads[0] = (struct quadpart){offsx, origoffst, color3};
        ck->quads[1] = (struct quadpart){offsx, origoffsy, color3};
        ck->quads[2] = (struct quadpart){offsr, origoffsy, color3};
        ck->quads[3] = (struct quadpart){offsr, origoffst, color3};
        
        if(__builtin_expect(vtxidx == vertexsize, 0))
        {
            FlushToilet();
            //vtxidx = 0;
        }
        
        ck = quads + (vtxidx++);
        
        const float widthmagic =
        #ifdef PIANOKEYS
            150.0F * keymul / 1280.0F
        #else
            128.0F * keymul / 1280.0F
        #endif
        ;
        offsx += widthmagic;
        offsr -= widthmagic;
        
        ck->quads[0] = (struct quadpart){offsx, offst, color1};
        ck->quads[1] = (struct quadpart){offsx, offsy, color1};
        ck->quads[2] = (struct quadpart){offsr, offsy, color2};
        ck->quads[3] = (struct quadpart){offsr, offst, color2};
        
    #else
        ck->quads[0] = (struct quadpart){offsx, offst, color1};
        ck->quads[1] = (struct quadpart){offsx, offsy, color1};
        ck->quads[2] = (struct quadpart){offsr, offsy, color1/*color2*/};
        ck->quads[3] = (struct quadpart){offsr, offst, color1/*color2*/};
    #endif
        
    #ifdef OUTLINE
    }
    #endif
    
    #endif
}
#else
static void AddRawVtx(float offsy, float offst, float offsx, float offsr, KCOLOR color1)
{
     if(vtxidx == vertexsize)
    {
        FlushToilet();
        //vtxidx = 0;
    }
    
    //offsx -= 0.125F;
    
    struct quad* __restrict ck = quads + (vtxidx++);
    
    #ifdef ROUNDEDGE
    //DWORD color = colortable[dwUID];
    //DWORD color1 = color; //(color & 0xFEFEFEFE) >> 1 | (1 << 31);
    
    #ifdef PFAKEY
        #ifndef HDR
        KCOLOR color2 = ((color1 & 0xFEFEFEFE) >> 1) | (0xFF << 24);
        #else
        KCOLOR color2 = (KCOLOR){
            color1.r * 0.5F,
            color1.g * 0.5F,
            color1.b * 0.5F,
            1.0F
        };
        #endif
    #else
        KCOLOR color2 = color1;
    #endif
    #ifndef HDR
    KCOLOR color3 = ((color1 & 0xFCFCFCFC) >> 2) | (0xFF << 24);
    #else
    KCOLOR color3 = (KCOLOR){
        color1.r * 0.25F,
        color1.g * 0.25F,
        color1.b * 0.25F,
        1.0F
    };
    #endif
    KCOLOR color = color1;
    //KCOLOR color2 = color1;
    //KCOLOR color3 = color;
    
    float middx = offsx + ((offsr - offsx) * 0.75F);
    float middy = offsy + ((offst - offsy) * 0.75F);
    
    /*
        0 - 1 - 2
        | /   \ |
        3 - 8 - 4
        | \   / |
        5 - 6 - 7
    */
    
    ck->quads[0] = (struct quadpart){offsx, offst, color1};
    ck->quads[1] = (struct quadpart){middx, offst, color2};
    ck->quads[2] = (struct quadpart){offsr, offst, color1};
    
    ck->quads[3] = (struct quadpart){offsx, middy, color1};
    ck->quads[4] = (struct quadpart){offsr, middy, color1};
    
    ck->quads[5] = (struct quadpart){offsx, offsy, color1};
    ck->quads[6] = (struct quadpart){middx, offsy, color2};
    ck->quads[7] = (struct quadpart){offsr, offsy, color1};
    
    ck->quads[8] = (struct quadpart){middx, middy, color3};
    
    /*float fquad = offsy + ((offst - offsy) * 0.75F);
    float lquad = offsy + ((offst - offsy) * 0.25F);
    
    ck->quads[8] = (struct quadpart){offsx, lquad, color1};
    ck->quads[9] = (struct quadpart){offsx, fquad, color1};
    
    ck->quads[0xA] = (struct quadpart){offsr, lquad, color3};
    ck->quads[0xB] = (struct quadpart){offsr, fquad, color3};*/
    
    #else
    
    //KCOLOR color1 = colortable[dwUID];
    
    #ifdef OUTLINE
    
    #ifndef HDR
    KCOLOR color3 = ((color1 & 0xFCFCFCFC) >> 2) | (0xFF << 24);
    #else
    KCOLOR color3 = (KCOLOR){
        color1.r * 0.25F,
        color1.g * 0.25F,
        color1.b * 0.25F,
        1.0F
    };
    #endif
    
    ck->quads[0] = (struct quadpart){offsx, offst, color3};
    ck->quads[1] = (struct quadpart){offsx, offsy, color3};
    ck->quads[2] = (struct quadpart){offsr, offsy, color3};
    ck->quads[3] = (struct quadpart){offsr, offst, color3};
    
    ck++;
    vtxidx++;
    
    offsy += minheight;
    offst -= minheight;
    if(offst < offsy)
    {
        ck->quads[0] = (struct quadpart){0, 0};
        ck->quads[1] = (struct quadpart){0, 0};
        ck->quads[2] = (struct quadpart){0, 0};
        ck->quads[3] = (struct quadpart){0, 0};
    }
    else
    {
        const float widthmagic =
        #ifdef PIANOKEYS
            150.0F * keymul / 1280.0F
        #else
            128.0F * keymul / 1280.0F
        #endif
        ;
        offsx += widthmagic;
        offsr -= widthmagic;
    #endif
        #ifdef PFAKEY
            #ifndef HDR
            KCOLOR color2 = ((color1 & 0xFEFEFEFE) >> 1) | (0xFF << 24);
            #else
            KCOLOR color2 = (KCOLOR){
                color1.r * 0.5F,
                color1.g * 0.5F,
                color1.b * 0.5F,
                1.0F
            };
            #endif
        #else
            KCOLOR color2 = color1;
        #endif
        
        ck->quads[0] = (struct quadpart){offsx, offst, color1};
        ck->quads[1] = (struct quadpart){offsx, offsy, color1};
        ck->quads[2] = (struct quadpart){offsr, offsy, color2};
        ck->quads[3] = (struct quadpart){offsr, offst, color2};
        
    #ifdef OUTLINE
    }
    #endif
    
    #endif
}
#endif

#ifdef PIANOKEYS
static inline void pianokey(float* __restrict offsx, float* __restrict offsr, BYTE num)
{
    int div = num / 12;
    int mod = num % 12;
    
    const BYTE keyoffssesses[] =
    {
        0, 4, 8, 14, 16, 24, 28, 32, 37, 40, 46, 48
    };
    num = keyoffssesses[mod];
    
    if(mod > 4)
        mod++;
    
    DWORD rawoffs = (div * 7 * keymul) + num;
    
    *offsx = rawoffs;
    *offsr = rawoffs + ((mod & 1) ? (keymul - (keymul >> 2)) : keymul);
}
#endif

static __attribute__((noinline)) void AddVtx(NoteNode localnode, ULONGLONG currtick, float tickscale)
{
    #ifdef PIANOKEYS
    float offsx;
    float offsr;
    pianokey(&offsx, &offsr, localnode.uid);
    #else
    DWORD rawoffs = localnode.uid & 0xFF;
    float offsx = rawoffs;
    float offsr = rawoffs + 1;
    #endif
    
    float offsy = -1.0F;
    //if(localnode.start > currtick)
        offsy = ((float)((int)(localnode.start - currtick)) * tickscale) - 1.0F;
    float offst = 1.0F;
    if(~localnode.end)
    {
        offst = ((float)((int)(localnode.end - currtick)) * tickscale) - 1.0F;
        #ifdef TRANSFORM
        if(offst > 1.0F)
            offst = 1.0F;
        #endif
    }
    
    AddRawVtx(offsy, offst, offsx, offsr, colortable[localnode.uid >> 8]);
}

static __attribute__((noinline)) void AddWideVtx(ULONGLONG start, float height, ULONGLONG currtick, float tickscale, DWORD range, DWORD color)
{
    #ifdef PIANOKEYS
    float offsx;
    float offsr;
    float dummy;
    pianokey(&offsx, &dummy, (BYTE)(range >> 8));
    pianokey(&dummy, &offsr, (BYTE)(range >> 0));
    #else
    float offsx = (BYTE)(range >> 8);
    float offsr = (BYTE)(range >> 0);
    #endif
    
    float offsy = -1.0F;
    //if(localnode.start > currtick)
        offsy = ((float)((int)(start - currtick)) * tickscale) - 1.0F;
    float offst = offsy + height;
    
    #ifndef HDR
    KCOLOR kc = color;
    #else
    KCOLOR kc;
    kc.r = (BYTE)(color >> 0) / 255.0F;
    kc.g = (BYTE)(color >> 8) / 255.0F;
    kc.b = (BYTE)(color >> 16) / 255.0F;
    kc.a = (BYTE)(color >> 24) ? ((BYTE)(color >> 24)) / 255.0F : 1.0F;
    #endif
    
    AddRawVtx(offsy, offst, offsx, offsr, kc);
}

#if defined(PFACOLOR)
static __attribute__((noinline)) KCOLOR HSV2RGB(float hue, float saturation, float value)
{
    float Irgb = saturation * value;
    float m = value - Irgb;
    
    hue *= 6;
    
    double r = 0;
    double g = 0;
    double b = 0;
    
    double frac = hue - (int)hue;
    double rrac = 1.0F - frac;
    
    switch((int)hue)
    {
        case 0:
            r = Irgb;
            g = frac * Irgb;
            break;
        case 1:
            r = rrac * Irgb;
            g = Irgb;
            break;
        case 2:
            g = Irgb;
            b = frac * Irgb;
            break;
        case 3:
            g = rrac * Irgb;
            b = Irgb;
            break;
        case 4:
            r = frac * Irgb;
            b = Irgb;
            break;
        case 5:
            r = Irgb;
            b = rrac * Irgb;
            break;
        default: // 1.0
            r = Irgb;
            break;
    }
    
    #ifndef HDR
    DWORD dwColor = 0;
    dwColor |= (BYTE)(((r + m) * 255.0F) + 0.5F) << 0;
    dwColor |= (BYTE)(((g + m) * 255.0F) + 0.5F) << 8;
    dwColor |= (BYTE)(((b + m) * 255.0F) + 0.5F) << 16;
    return dwColor;
    #else
    return (KCOLOR){ r + m, g + m, b + m, 1.0F };
    #endif
}
#endif

#ifdef O3COLOR
#ifdef HDR
const int dominodark[] =
{
    0xFFA5A5, //red
    0xFFBD55, //orang
    0xFFFF2D, //yellow
    0x8DFF55, //green
    0x55FFE2, //blue
    //0x96D7F1, //(doesn't exist)
    0xD7AFFF, //lavander
    0xFF91FB  //pink
};
//0xAAC8EF, //(doesn't exist)

/*
const int dominolight[] =
{
    0xFFA5A5, //red
    0xFFBD55, //orang
    0xFFFF2D, //yellow
    0x8DFF55, //green
    0x55FFE2, //blue
    0xD7AFFF, //lavander
    0xFF91FB  //pink
};*/
#else
/*
const KCOLOR dominodark[] =
{
    0x000078,
    0x004A78,
    0x007878,
    0x007828,
    0x647800,
    0x78003C,
    0x740078
};
*/

// shitty replacement values
const KCOLOR dominodark[] =
{
    0xD10000,
    0xAB6900,
    0x969600,
    0x39AB00,
    0x00AB8E,
    //0x1883AD,
    0x6B00D6,
    //0xC700C0
};


/*
const KCOLOR dominolight[] =
{
    0xBBBBFF,
    0x7FCDFF,
    0x61FFFF,
    0x7FFFA9,
    0xE9FF7F,
    0xFFC3E1,
    0xFCACFF
};
*/
#endif
#endif

#ifdef KEYBOARD

static inline KCOLOR LerpColor(KCOLOR color, KCOLOR def, float a)
{
    //printf("LerpColor %f\n", a);
    
    #ifndef HDR
    float cr = (BYTE)color;
    float cg = (BYTE)(color >> 8);
    float cb = (BYTE)(color >> 16);
    
    float dr = (BYTE)def;
    float dg = (BYTE)(def >> 8);
    float db = (BYTE)(def >> 16);
    #else
    float cr = color.r;
    float cg = color.g;
    float cb = color.b;
    
    float dr = def.r;
    float dg = def.g;
    float db = def.b;
    #endif
    
    cr += (dr - cr) * a;
    cg += (dg - cg) * a;
    cb += (db - cb) * a;
    
    #ifndef HDR
    if(cr < 0) cr = 0; else if(cr > 255.49F) cr = 255.49F;
    if(cg < 0) cg = 0; else if(cg > 255.49F) cg = 255.49F;
    if(cb < 0) cb = 0; else if(cb > 255.49F) cb = 255.49F;
    
    return (BYTE)cr | ((DWORD)cg << 8) | ((DWORD)cb << 16) | (color & (0xFF << 24));
    #else
    return (KCOLOR){ cr, cg, cb, color.a };
    #endif
}

#endif

static DWORD trackcount;

#ifdef WMA_SIZE
ULONGLONG* fps_wma;
DWORD fps_wmai;
#endif

static BOOL isrender;

DWORD WINAPI RenderThread(PVOID lpParameter)
{
    puts("Hello from renderer");
    
    isrender = FALSE;
    
    wglMakeCurrent(dc, glctx);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallbackARB(DebugCB, NULL);
    
    #if !defined(TIMI_CAPTURE) || defined(TIMI_NOWAIT)
    uglSwapControl(1);
    if(!uglSwapControl(-1))
    {
        puts("Adaptive VSync is not available");
        uglSwapControl(1);
    }
    //uglSwapControl(0);
    #else
    uglSwapControl(0);
    #endif
    
    glViewport(0, 0, 1280, 720);
    
#ifdef TIMI_CAPTURE
    const POINT capsize =
    {
        .x = CAPW,
        .y = CAPH
    };
    
    glViewport(0, 0, capsize.x, capsize.y);
    
    GLuint cap_fb = 0;
    glGenFramebuffers(1, &cap_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, cap_fb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, cap_fb);
    
    if(uglSupportsExt("WGL_EXT_framebuffer_sRGB"))
        glEnable(GL_FRAMEBUFFER_SRGB);
    
    GLuint cap_rb = 0;
    glGenRenderbuffers(1, &cap_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, cap_rb);
    glRenderbufferStorage(GL_RENDERBUFFER,
    //#ifdef HDR
        uglSupportsExt("WGL_EXT_framebuffer_sRGB")
        ? GL_SRGB8_ALPHA8
        :
    //#endif
        GL_RGBA8
        ,
        capsize.x, capsize.y);
    
    #ifdef TRANSFORM
    GLuint cap_db = 0;
    glGenRenderbuffers(1, &cap_db);
    glBindRenderbuffer(GL_RENDERBUFFER, cap_db);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
        capsize.x, capsize.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cap_db);
    
    glBindRenderbuffer(GL_RENDERBUFFER, cap_rb);
    #endif
    
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, cap_rb);
    
    
    #ifndef TIMI_NOCAPTURE
    HANDLE hpipe = CreateNamedPipeW
    (
        L"\\\\.\\pipe\\MPGL_readpixels",
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE,
        1,
        0,
        0,
        0,
        NULL
    );
    
    if(!hpipe || hpipe == INVALID_HANDLE_VALUE)
    {
        puts("Pipe error");
        PostQuitMessage(1);
        CloseWindow(glwnd);
        return GetLastError();
    }
    
    void* capdata = malloc(capsize.x * capsize.y * 4);
    #endif
#endif

#if defined(HDR) && !defined(FASTHDR)
    (void)NvOptimusEnablement;
#endif
    
    notelist = 0;
    freelist = 0;
    freelist_head = 0;
    freelist_return = 0;
    freelist_returnhead = 0;
    returnfailcount = 0;
    VisibleNoteList = 0;
    VisibleNoteListHead = 0;
    notealloccount = 0;
    currnotealloc = 0;
    
    ply = *(MMPlayer**)lpParameter;
    player = ((MMPlayer**)lpParameter)[1];
    
    midisize += 2 * sizeof(MMPlayer);
    
    do
    {
        MMTrack* trk = ply->tracks;
        while((trk++)->ptrs)
            trackcount++;
    }
    while(0);
    
    trackcount <<= 4; // *= 16;
    
    colortable = malloc(trackcount * sizeof(*colortable));
    if(!colortable)
        puts("No colortable, fuck");
    
    midisize += sizeof(*colortable) * trackcount;
    
    do
    {
        DWORD seed = 1;
        
    #ifdef PFACOLOR
        seed = 56;
    #endif
        
        DWORD i = 0;
        
        while(i != trackcount)
        {
            DWORD ic = 3;
            
            int col = 0xFF;
            
    #ifdef PFACOLOR
            DWORD seeds[3];
    #endif
            
            while(ic--)
            {
                seed *= 214013;
                seed += 2531011;
                
    #ifdef PFACOLOR
                seeds[2 - ic] = seed >> 16;
    #endif
                
                col = ((seed & 0x7F) + 80) | (col << 8);
            }
    
    #ifdef O3COLOR
        col = dominodark[(((i >> 4) + 6) % (sizeof(dominodark)/sizeof(*dominodark)))] | (0xFF << 24);
        
        #ifdef HDR
        col = (((col >>  0) & 0xFF) << 16)
            | (((col >>  8) & 0xFF) <<  8)
            | (((col >> 16) & 0xFF) <<  0)
            | (0xFF << 24);
        #endif
        
        ic = 16;
        while(ic--)
        #ifndef HDR
            colortable[i++] = col;
        #else
            colortable[i++] = (KCOLOR){
                (BYTE)(col >> 0) * (1.0F / 255.0F),
                (BYTE)(col >> 8) * (1.0F / 255.0F),
                (BYTE)(col >> 16) * (1.0F / 255.0F),
                1.0F
            };
        #endif
    #else
    #ifdef PFACOLOR
            float light = ((seeds[0] % 20) + 80) / 100.0F;
        #ifdef ROUNDEDGE
            float sat = ((seeds[1] % 64) + 36) / 100.0F;
        #else
            float sat = ((seeds[1] % 40) + 60) / 100.0F;
        #endif
            colortable[i++] = HSV2RGB((seeds[2] % 360) / 360.0F, sat, light)
            #ifndef HDR
                | (0xFF << 24)
            #endif
            ;
    #else
        #ifndef HDR
            colortable[i++] = col;
        #else
            colortable[i++] = (KCOLOR){
                (BYTE)(col >> 0) * (1.0F / 255.0F),
                (BYTE)(col >> 8) * (1.0F / 255.0F),
                (BYTE)(col >> 16) * (1.0F / 255.0F),
                1.0F
            };
        #endif
    #endif
    #endif
        }
    }
    while(0);
    
#ifdef TRACKID
    trackcount <<= 8; // *= 256;
#else
    trackcount = 16 * 256;
#endif
    
    ActiveNoteList = malloc(sizeof(size_t) * trackcount);
    if(!ActiveNoteList)
        puts("No ActiveNoteList, fuck");
    ZeroMemory(ActiveNoteList, sizeof(size_t) * trackcount);
    
    midisize += trackcount * sizeof(size_t);
    
    //UNUSED for now
    //trackcount >>= 12; // undo 16 * 256
    
    grInstallShader();
    
    #ifdef GLTEXT
    grfInstallShader();
    #endif
    
    quads = 0;
    GLuint* indexes = 0;
    
    for(;;)
    {
        quads = calloc(vertexsize, sizeof(*quads));
        if(quads)
        {
            size_t dest = vertexsize * NOTEVTX;
            
            indexes = malloc(dest * sizeof(GLuint));
            if(indexes)
            {
                midisize += vertexsize * sizeof(*quads) * 2;
                midisize += sizeof(GLuint) * dest;
                
                size_t vidx = 0;
                
                for(size_t i = 0; i != dest;)
                {
                #ifdef ROUNDEDGE
                    /*
                        0 - 1 - 2
                        | /   \ |
                        3 - 8 - 4
                        | \   / |
                        5 - 6 - 7
                    */
                    
                    //top left
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 0;
                    
                    //top right
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 4;
                    indexes[i++] = vidx + 2;
                    
                    //bottom left
                    indexes[i++] = vidx + 6;
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 5;
                    
                    //bottom right
                    indexes[i++] = vidx + 4;
                    indexes[i++] = vidx + 6;
                    indexes[i++] = vidx + 7;
                    
                    //TL1
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 1;
                    
                    //TL2
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 3;
                    
                    //TR1
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 4;
                    
                    //TR2
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 4;
                    indexes[i++] = vidx + 1;
                    
                    //BL1
                    indexes[i++] = vidx + 6;
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 3;
                    
                    //BL2
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 6;
                    
                    //BR1
                    indexes[i++] = vidx + 4;
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 6;
                    
                    //BR2
                    indexes[i++] = vidx + 8;
                    indexes[i++] = vidx + 6;
                    indexes[i++] = vidx + 4;
                #else
                    /*
                        0 - 3
                        | \ |
                        1 - 2
                    */
                    
                    /*
                    indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 0;
                    */
                    
                    //fucking trigger indexes 
                    
                    /*indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 1;
                    */
                    /*indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 3;*/
                    
                    
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 1;
                    
                #endif
                    
                    vidx += QUADCOUNT;
                }
                break;
            }
            else
            {
                free(quads);
                quads = 0;
            }
        }
        
        //vertexsize >>= 1;
        //if(!vertexsize)
        {
            puts("Can't allocate vertex index memory!");
            break;
        }
    }
    
    if(!quads)
    {
        puts("Failed to callocate quads");
        goto ded;
    }
    
    GLuint g_vao = 0;
    
    if(glGenVertexArrays && glBindVertexArray)
    {
        glGenVertexArrays(1, &g_vao);
        glBindVertexArray(g_vao);
    }
    
    GLuint g_vbo;
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    
    GLuint g_ebo;
    glGenBuffers(1, &g_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexsize * NOTEVTX * sizeof(GLuint), 0, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertexsize * NOTEVTX * sizeof(GLuint), indexes);
    free(indexes);
    indexes = 0;
    
    printf("VBO note buffer# %lu\n", (DWORD)vertexsize);
    
    size_t datasize = vertexsize * sizeof(*quads);
    glBufferData(GL_ARRAY_BUFFER, datasize, 0, GL_STREAM_DRAW);
    
    vtxidx = 0;
        
    /*quads[0].tl = (struct quadpart){ 1.0F,  1.0F, 0x00000000};
    quads[0].bl = (struct quadpart){-1.0F,  1.0F, 0xFF00FF00};
    quads[0].br = (struct quadpart){-1.0F, -1.0F, 0xFF00FFFF};
    quads[0].tr = (struct quadpart){ 1.0F, -1.0F, 0xFF0000FF};
    vtxidx++;
    
    quads[1].tl = (struct quadpart){ 0.5F,  0.5F, 0x00000000};
    quads[1].bl = (struct quadpart){-0.5F,  0.5F, 0xFF00FF00};
    quads[1].br = (struct quadpart){-0.5F, -0.5F, 0xFF00FFFF};
    quads[1].tr = (struct quadpart){ 0.5F, -0.5F, 0xFF0000FF};
    vtxidx++;
    
    quads[2].tl = (struct quadpart){ 0.25F,  0.25F, 0x00000000};
    quads[2].bl = (struct quadpart){-0.25F,  0.25F, 0xFF00FF00};
    quads[2].br = (struct quadpart){-0.25F, -0.25F, 0xFF00FFFF};
    quads[2].tr = (struct quadpart){ 0.25F, -0.25F, 0xFF0000FF};
    vtxidx++;*/
    
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    ply->SyncPtr = &player->RealTime;
    ply->SyncOffset = 50000000;
    ply->KShortMsg = dwEventCallback;
    ply->KLongMsg = LongMessage;
    ply->KSyncFunc = NoteReturn;
    
    player->SleepTimeMax = 0;
    player->SleepTicks = 1;
    player->KLongMsg = dwLongMsg;
    
    
    #ifdef TIMI_CAPTURE
    ULONGLONG timersync = ~0;
    timersync = timersync >> 2;
    
    #ifndef TIMI_NOCAPTURE
    player->SyncPtr = (LONGLONG*)&timersync;
    player->SyncOffset = 0;
    #endif
    
    #ifndef TIMI_NOWAIT
    #ifdef TIMI_SILENT
    player->KShortMsg = FPS_ShortMsg;
    #endif
    player->KSyncFunc = FPS_SyncFunc;
    #endif
    
    ply->SleepTicks = 1;
    ply->SleepTimeMax = 0;
    #endif
    
    #if defined(TEXTNPS)
    kNPOriginal = player->KShortMsg;
    kNSOriginal = player->KSyncFunc;
    player->KShortMsg = kNPIntercept;
    player->KSyncFunc = kNPSync;
    #elif defined(DYNASCROLL)
    player->KSyncFunc = NoteSync;
    
    syncvalue = 0;
    mmnotesync = 0;
    #endif
    #ifdef TIMI_TIEMR
    ULONGLONG timersync = ~0;
    timersync = timersync >> 2;
    do
    {
        void(WINAPI*timisettimer)(void* func) = (void*)GetProcAddress(KSModule, "timidrwSetTimeFunc");
        if(timisettimer)
        {
            timisettimer((void*)timiTimerFunc);
            player->SyncPtr = (LONGLONG*)&timersync;
            player->SyncOffset = 0;
        }
    }
    while(0);
    #endif
    
    #ifdef TIMI_CAPTURE
    if(!player->KShortMsg)
        player->KShortMsg = FPS_ShortMsg;
    #endif
    
    CreateThread(0, 0x4000, PlayerThread,    ply, 0, 0);
    //CreateThread(0, 0x4000, PlayerThread, player, 0, 0);
    
    int(WINAPI*NtQuerySystemTime)(QWORD* timeptr) = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtQuerySystemTime");
    
    QWORD prevtime;
    NtQuerySystemTime(&prevtime);
    QWORD currtime = prevtime;
    
    DWORD timeout = 0;
    
    #if defined(ROUNDEDGE) //|| defined(TRIPPY) //|| defined(GLOWEDGE)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    #endif
    
    #ifdef TRANSFORM
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    #else
    //glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LEQUAL);
    #endif
    
    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_CCW);
    
    #ifdef KEYBOARD
    ULONGLONG lasttimer = 0;
    ULONGLONG currtimer = 0;
    
    NoteNode KeyNotes[256];
    ZeroMemory(KeyNotes, sizeof(KeyNotes));
    
    midisize += sizeof(KeyNotes);
    #endif
    
    #ifdef TIMI_CAPTURE
    FPS_capture = TRUE;
    #endif
    
    #if defined(WMA_SIZE) && defined(GLTEXT)
    fps_wma = malloc(sizeof(*fps_wma)* WMA_SIZE);
    memset(fps_wma, 0, sizeof(*fps_wma) * WMA_SIZE);
    fps_wmai = 0;
    #endif
    
    while(!(WaitForSingleObject(vsyncevent, timeout) >> 9))
    {
        if(canrender && !isrender)
        {
            CreateThread(0, 0x4000, PlayerThread, player, 0, 0);
            
            isrender = TRUE;
            
            #if defined(TIMI_CAPTURE) && !defined(TIMI_NOCAPTURE)
            
            player->SyncPtr = (LONGLONG*)&timersync;
            player->SyncOffset = 0;
            
            //player->KShortMsg = FPS_ShortMsg;
            //player->KSyncFunc = FPS_SyncFunc;
            #endif
        }
        
        #if defined(TIMI_CAPTURE) && !defined(TIMI_NOWAIT)
        if(!FPS_capture && player->tracks->ptrs)
        {
            timeout = 1;
            continue;
        }
        
        timeout = 0;
        #endif
        
        ResetEvent(vsyncevent);
        
        #ifdef TIMI_CAPTURE
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cap_fb);
        glViewport(0, 0, capsize.x, capsize.y);
        #endif
        
        glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        //glClearDepth(1.0);
        //glClearColor(0.5859375F, 0.5859375F, 0.5859375F, 0.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if(glBindVertexArray)
            glBindVertexArray(g_vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        
        //goto topkek;
        
        glEnableVertexAttribArray(attrGrVertex);
        glEnableVertexAttribArray(attrGrColor);
        
        glVertexAttribPointer(attrGrVertex, 2, GL_FLOAT, GL_FALSE, sizeof(struct quadpart), 0);
        #ifndef HDR
        glVertexAttribPointer(attrGrColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct quadpart), (void*)8);
        #else
        glVertexAttribPointer(attrGrColor, 4, GL_FLOAT, GL_FALSE, sizeof(struct quadpart), (void*)8);
        #endif
        
        glUseProgram(shGrShader);
        if(glBindVertexArray)
            glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
        
        BIND_IF(glUniform1f, attrGrNotemix, 0.0F);
        
        #ifdef GLTEXT
        drawnotesraw = 0;
        #endif
        
        #ifdef DYNASCROLL
        ULONGLONG notesync = mmnotesync;
        ULONGLONG currtick = syncvalue;
        #else
        DWORD tickheight =
        #ifdef CUSTOMTICK
        CUSTOMTICK
        ;
        #else
        (2500000
        #if defined(TRANSFORM) || defined(ROTAT)
        * 4
        #endif
        ) / player->tempomulti;
        #endif
        ULONGLONG notesync = TICKVAR;
        ULONGLONG currtick = TICKVAL;
        float tickscale = 1.0F / (float)tickheight;
        #endif
        
        #ifdef KEYBOARD
        currtimer = player->RealTime;
        #endif
        
        #ifdef SHTIME
        #ifdef DYNASCROLL
        BIND_IF(glUniform1f, uniGrTime, (float)(currtick / (double)tickheight * 0.25));
        #else
        BIND_IF(glUniform1f, uniGrTime, (float)((double)(player->RealTime) / 1e7));
        #endif
        #endif
        
        ULONGLONG midtick = currtick + tickheight;
        ULONGLONG toptick = midtick + tickheight;
        
        //printf("%10llu %10llu %i %u\n", notesync, currtick, (int)(notesync - currtick), player->tempo);
        
        // ===[BAD IDEA NEVER UNCOMMENT THIS]===
        /*if(!timeout && notesync < midtick)
        {
            midtick = notesync;
            toptick = midtick + tickheight;
            currtick = midtick - tickheight;
        }*/
        // ===[BAD IDEA NEVER UNCOMMENT THIS]===
        
        #if defined(HDR) && !defined(FASTHDR)
        #ifdef TRIPPY
        const float uc = 0.1F;
        const float kc = -1.0F;
        #else
        const float uc = 0.0F;
        const float kc = -1.0F;
        #endif
        #ifdef ROTAT
        AddRawVtx(kc, 1,   0, 150, (KCOLOR){uc, uc, uc, 1});
        AddRawVtx(kc, 1, 150, 300, (KCOLOR){uc, uc, uc, 1});
        AddRawVtx(kc, 1, 300, 450, (KCOLOR){uc, uc, uc, 1});
        AddRawVtx(kc, 1, 450, 600, (KCOLOR){uc, uc, uc, 1});
        #else
        #ifdef NOKEYBOARD
        AddRawVtx(kc - 1.5, 1.5, 0, 600, (KCOLOR){uc, uc, uc, 1});
        #else
        AddRawVtx(kc, 1, 0, 600, (KCOLOR){uc, uc, uc, 1});
        #endif
        #endif
        #endif
        
        
        if(vtxidx)
            FlushToilet();
        
        BIND_IF(glUniform1f, attrGrNotemix, 1.0F);
        
        #if 1 && defined(DEBUGTEXT) && defined(NOISEOVERLAY)
        {
            LONGLONG asdtick = currtick - (currtick % player->timediv) - player->timediv - player->timediv;
            do
            {
                asdtick += player->timediv;
                
                for(int asdi = 0; asdi != 0x80; asdi++)
                {
                    NoteNode debugnode;
                    debugnode.next = 0;
                    debugnode.uid = asdi;
                    debugnode.start = asdtick;
                    debugnode.end = asdtick + player->timediv;
                    
                    AddVtx(debugnode, currtick, tickscale);
                    
                    //AddWideVtx(currtick, 2.0F, currtick, tickscale, asdi | (asdi << 8), 0xFF7F7F7F);
                }
                
                AddWideVtx((ULONGLONG)asdtick, 1.0F / 64.0F, currtick, tickscale, 0x7F, -1);
                
            }
            while((LONGLONG)(asdtick - toptick) < 0);
            
            if(vtxidx)
                FlushToilet();
        }
        #endif
        
        NoteNode* prevnote = 0;
        NoteNode* currnote = VisibleNoteList;
        
        #ifndef NORENDEROPT
        //loop over freeable notes first
        while(currnote && currnote->start <= currtick) // already triggered
        {
            #ifdef KEYBOARD
            NoteNode* lmn = &KeyNotes[(BYTE)currnote->uid];
            
            if(lmn->uid != currnote->uid)
            {
                lmn->uid = currnote->uid;
                lmn->start = 14000000;
                //lmn->start = 21000000;
            }
            else if(lmn->start < 10000000)
                lmn->start = 10000000;
            #endif
            
            if(currnote->end > currtick) //note not finished yet
            {
                NoteNode localnode = *currnote;
                prevnote = currnote;
                currnote = localnode.next;
                
                AddVtx(localnode, currtick, tickscale);
            }
            else
            {
                if(prevnote)
                {
                    NoteNode* nn = currnote->next;
                    prevnote->next = nn;
                    
                    NoteFree(currnote);
                    currnote = nn;
                    
                    continue;
                }
                else if(VisibleNoteList == currnote)
                {
                    /*
                    if(VisibleNoteList == VisibleNoteListHead)
                    {
                        NoteNode* nextnode = currnote->next;
                        VisibleNoteList = nextnode;
                        //VisibleNoteListHead = 0;
                        
                        NoteFree(currnote);
                        currnote = nextnode;
                        break;
                    }
                    */
                    
                    prevnote = 0;
                    
                    NoteNode* nn = currnote->next;
                    VisibleNoteList = nn;
                    NoteFree(currnote);
                    currnote = nn;
                    continue;
                }
                else
                {
                    puts("WAT HOW");
                    break;
                }
            }
        }
        
        //loop over visible notes which don't require special processing
        while(currnote && currnote->start < toptick)
        {
            NoteNode localnode = *currnote;
            prevnote = currnote;
            currnote = localnode.next;
            
            AddVtx(localnode, currtick, tickscale);
        }
        #else
        
        while(currnote)
        {
            if(currnote->end > currtick)
            {
                NoteNode localnode = *currnote;
                prevnote = currnote;
                currnote = localnode.next;
                
                if(localnode.start < toptick)
                    AddVtx(localnode, currtick, tickscale);
                
                #ifdef KEYBOARD
                if(localnode.start <= currtick)
                {
                    NoteNode* lmn = &KeyNotes[(BYTE)localnode.uid];
                    
                    if(lmn->uid != localnode.uid)
                    {
                        lmn->uid = localnode.uid;
                        lmn->start = 14000000;
                        //lmn->start = 21000000;
                    }
                    else if(lmn->start < 10000000)
                        lmn->start = 10000000;
                }
                #endif
            }
            else
            {
                if(prevnote)
                {
                    NoteNode* nn = currnote->next;
                    prevnote->next = nn;
                    
                    NoteFree(currnote);
                    currnote = nn;
                    
                    continue;
                }
                else if(VisibleNoteList == currnote)
                {
                    /*
                    if(VisibleNoteList == VisibleNoteListHead)
                    {
                        VisibleNoteList = 0;
                        VisibleNoteListHead = 0;
                        
                        if(currnote->next)
                            puts("Oh no, eventual consistency is broken!");
                        
                        NoteFree(currnote);
                        currnote = 0;
                        break;
                    }*/
                    
                    prevnote = 0;
                    
                    NoteNode* nn = currnote->next;
                    VisibleNoteList = nn;
                    NoteFree(currnote);
                    currnote = nn;
                    continue;
                }
                else
                {
                    puts("WAT HOW");
                    break;
                }
            }
        }
        
        #endif
        
        if(vtxidx)
            FlushToilet();
        
        #ifdef GLTEXT
        drawnotes = drawnotesraw + vtxidx;
        #endif
        
        #ifdef DEBUGTEXT
        BIND_IF(glUniform1f, attrNotemix, 0.0F);
        
        LONGLONG debugtick = currtick - (currtick % player->timediv) - player->timediv - player->timediv;
        do
        {
            debugtick += player->timediv;
            
            NoteNode debugnode;
            debugnode.next = 0;
            debugnode.uid = 0;
            debugnode.start = debugtick;
            debugnode.end = debugtick + (player->timediv >> 1) + (player->timediv >> 2);
            
            AddVtx(debugnode, currtick, tickscale);
            
            AddWideVtx((ULONGLONG)debugtick, 1.0F / 64.0F, currtick, tickscale, 0x7F, -1);
        }
        while((LONGLONG)(debugtick - toptick) < 0);
        
        if(vtxidx)
            FlushToilet();
        #endif
        
        if(!freelist_return)
        {
            freelist_returnhead = freelist_head;
            freelist_head = 0;
            freelist_return = freelist;
            freelist = 0;
        }
        
        /*if(TICKVAR < toptick)
        do
        {
            if(vtxidx == vertexsize)
            {
                FlushToilet();
                vtxidx = 0;
            }
            
            float pob = ((float)((int)(( TICKVAR ) - currtick)) * tickscale) - 1.0F;
            float pos = 1.0F;
            
            quads[vtxidx].quads[0] = (struct quadpart){-1.0F, pos, 0x11111111};
            quads[vtxidx].quads[1] = (struct quadpart){-1.0F, pob, 0x11111111};
            quads[vtxidx].quads[2] = (struct quadpart){ 1.0F, pob, 0x11111111};
            quads[vtxidx].quads[3] = (struct quadpart){ 1.0F, pos, 0x11111111};
            vtxidx++;
        }
        while(0);*/
        
        #ifdef KEYBOARD
        
        #if defined(HDR) && defined(TRIPPY)
        BIND_IF(glUniform1f, attrGrNotemix, 0.0F);
        //BIND_IF(glUniform1f, attrNotemix, 1.0F / 64.0F);
        #endif
        
        #if defined(PIANOBAR)
        AddRawVtx(-1.02, -0.98, 0, 600, (KCOLOR){1.0, 0.0, 0.0, 1});
        #endif
        
        ULONGLONG delta = 40000000;
        
        if((currtimer - lasttimer) < 2500000) // 250ms
            //delta = (currtimer - lasttimer) * 8;
            delta = (currtimer - lasttimer) * 6;
            //delta = (currtimer - lasttimer) * 3;
        
        #ifdef PIANOKEYS
        BOOL blackflag = FALSE;
        int keyflag = 0;
        int posflag = 0;
        #else
        #define posflag i
        #endif
        
        #ifdef HDR
        float shalpha[256];
        KCOLOR shcolor[256];
        #endif
        
        #ifdef WIDEMIDI
        for(int i = 0; i < 256;)
        #else
        for(int i = 0; i < 128;)
        //for(int i = 0; i < 256;)
        #endif
        {
            NoteNode* lmn = &KeyNotes[i];
            KCOLOR dwColor =
                #ifdef PIANOKEYS
                    blackflag
                #else
                    1
                #endif
                #ifdef GLOW
                #ifndef HDR
                ? 0xFF111111 : 0xFF808080;
                #else
                    #ifndef TRIPPY
                    ? (KCOLOR){ 0.04296875F, 0.04296875F, 0.04296875F, 1.0F }
                    : (KCOLOR){ 0.25F, 0.25F, 0.25F, 1.0F };
                    #else
                    ? (KCOLOR){ 0.06640625F, 0.06640625F, 0.06640625F, 1.0F }
                    : (KCOLOR){ 0.5F, 0.5F, 0.5F, 1.0F };
                    #endif
                #endif
                #else
                #ifdef HDR
                #error HDR is not supported if glow is turned off
                #endif
                ? 0xFF202020 : 0xFFFFFFFF;
                #endif
            
            if(lmn->start)
            {
                float coloralpha = (float)lmn->start * 0.0000001F;
                
                #ifdef GLOW
                if(!(
                #ifdef PIANOKEYS
                    blackflag
                #else
                    1
                #endif
                ))
                {
                    float grayalpha = coloralpha < 1.0F ? coloralpha : 1.0F;
                    #ifndef HDR
                    dwColor = LerpColor(dwColor, 0xFFFFFFFF, grayalpha);
                    #else
                    dwColor = LerpColor(dwColor, (KCOLOR){ 1.0F, 1.0F, 1.0F, 1.0F }, grayalpha);
                    #endif
                }
                #endif
                
                dwColor = LerpColor(dwColor, ((colortable[lmn->uid >> 8] )/*& 0xFEFEFEFE) >> 1*/), coloralpha)
                #ifndef HDR
                | (0xFF << 24)
                #endif
                ;
                
                #ifdef HDR
                shalpha[i] = coloralpha;
                //shcolor[i] = dwColor;
                shcolor[i] = colortable[lmn->uid >> 8];
                #endif
                
                if(lmn->start > delta)
                    lmn->start -= delta;
                else
                {
                    lmn->uid = 0;
                    lmn->start = 0;
                }
            }
            #ifdef HDR
            else
            {
                shalpha[i] = 0;
                shcolor[i] = (KCOLOR){0, 0, 0, 0};
            }
            #endif
            
            #ifdef PIANOKEYS
            float offsx, offsr;
            pianokey(&offsx, &offsr, i);
            #endif
            
            #ifndef NOKEYBOARD
            
            //AddRawVtx(1.0F, !blackflag ? 1.15F : 1.25F, posflag, posflag + 2, colortable[KeyNotes[i].uid >> 8]);
            AddRawVtx(
            #ifdef PIANOKEYS
                blackflag
            #else
                0
            #endif
                ?
            #if defined(TRANSFORM) && defined(ROTAT)
                -1.01875F : -1.125F
            #elif defined(TRANSFORM) || defined(ROTAT)
                -1.075F : -1.125F
            #else
                -1.30F : -1.5F
            #endif
                , -1.0F,
            #ifdef PIANOKEYS
                offsx, offsr
            #else
                posflag, posflag + 1
            #endif
                , dwColor);
            #endif
            
            #ifdef PIANOKEYS
            
            if(!blackflag) // is white key
            {
                if(keyflag != 0 && keyflag != 5) //is not first or middle key
                {
                    blackflag = TRUE; // set black status
                    keyflag -= 1;     // move back one key
                    i -= 1;           // move back one key
                    posflag -= 1;     // physically move back 0.5 white keys
                }
                else // skip black key
                {
                    keyflag += 2;     // move forward one white key
                    i += 2;           
                    posflag += 2;
                }
            }
            else // is a black key
            {
                blackflag = FALSE;    // set white status
                if(keyflag == 10)     // end-of-group
                {
                    keyflag = 0;
                    i += 2;           // no black key, so only advance 2
                }
                else if(keyflag == 3) // missing black key after this one
                {
                    keyflag += 2;
                    i += 2;
                }
                else
                {
                    keyflag += 3;
                    i += 3;           // move to white key after next white key
                }
                posflag += 3;         // physically move forward 1.5 white keys
            }
            
            #else
            i++;
            #endif
        }
        
        #ifdef HDR
        #ifdef WIDEMIDI
        BIND_IF(glUniform1fv, uniGrLightAlpha, 256, shalpha);
        BIND_IF(glUniform4fv, uniGrLightColor, 256, (GLfloat*)shcolor);
        #else
        BIND_IF(glUniform1fv, uniGrLightAlpha, 128, shalpha);
        BIND_IF(glUniform4fv, uniGrLightColor, 128, (GLfloat*)shcolor);
        #endif
        #endif
        
        lasttimer = currtimer;
        
        #endif
        
        if(vtxidx)
            FlushToilet();
        //printf("Drawn: %10llu | Desync: %10lli\n", notesdrawn, player->RealTime - ply->RealTime);
        //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        //glDrawArrays(GL_TRIANGLES, 0, 3);
        //glDrawElements(GL_TRIANGLES, 6 * vtxidx, GL_UNSIGNED_INT, indexes);s
        
        #ifndef GLTEXT
        if(notealloccount != currnotealloc)
        {
            currnotealloc = notealloccount;
            printf("Note allocation changed: %14llu\n", currnotealloc);
        }
        #endif
        
        glDisableVertexAttribArray(attrGrVertex);
        glDisableVertexAttribArray(attrGrColor);
        
    //topkek:
        
        #ifdef GLTEXT
        grfDrawFontOverlay();
        #endif
        
        glUseProgram(0);
        //glFlush();
        
        #ifndef TIMI_CAPTURE
        //InvalidateRect(glwnd, 0, 0);
        //uglSwapBuffers(dc);
        SwapBuffers(dc);
        //glFinish();
        
        glViewport(erect.left, erect.top, erect.right - erect.left, erect.bottom - erect.top);
        #endif
        
        /*
        #ifdef TIMI_CAPTURE
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(
            0, 0, capsize.x, capsize.y,
            erect.left, erect.top, erect.right -  erect.left, erect.bottom - erect.top,
            GL_COLOR_BUFFER_BIT, GL_LINEAR);
        #endif
        */
        
        NtQuerySystemTime(&currtime);
        #if !defined(TIMI_CAPTURE) || defined(TIMI_NOWAIT) // || defined(TIMI_NOCAPTURE)
        if((currtime - prevtime) > 10000)
            timeout = 0;
        else
            //timeout = 1;
            timeout = 0;
        #endif
        
        #if defined(WMA_SIZE) && defined(GLTEXT)
        fps_wma[fps_wmai] = currtime - prevtime;
        if(++fps_wmai == WMA_SIZE)
            fps_wmai = 0;
        #endif
        
        prevtime = currtime;
        
        #ifdef TIMI_CAPTURE
        #ifndef TIMI_NOWAIT
        if(FPS_capture)
        #endif
        {
            FPS_capture = FALSE;
            
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                0, 0, capsize.x, capsize.y,
                erect.left, erect.top, erect.right -  erect.left, erect.bottom - erect.top,
                GL_COLOR_BUFFER_BIT, BLITMODE);
            
            InvalidateRect(glwnd, 0, 0);
            uglSwapBuffers(dc);
            
            #ifndef TIMI_NOCAPTURE
            glReadPixels(
                0, 0, capsize.x, capsize.y,
                GL_BGRA, GL_UNSIGNED_BYTE,
                capdata
            );
            
            DWORD written;
            while(!WriteFile(hpipe, capdata, capsize.x * capsize.y * 4, &written, 0))
            {
                printf("Pipe error %08X\n", GetLastError());
                switch(GetLastError())
                {
                    case ERROR_NO_DATA:
                        puts("Pipe closed, dying");
                        goto ded;
                    
                    case ERROR_PIPE_LISTENING:
                    {
                        if(WaitForSingleObject(vsyncevent, 1000) >> 9)
                        {
                            puts("Changed your mind, eh?");
                            goto ded;
                        }
                        break;
                    }
                }
            }
            #endif
            
            glViewport(erect.left, erect.top, erect.right - erect.left, erect.bottom - erect.top);
        }
        #endif
        
        //#ifndef KEYBOARD
        if(player->tracks->ptrs)
            continue;
        
        #ifdef GRACE
        #ifdef KEYBOARD
        vtxidx = 0;
        for(DWORD i = 0; i != 128; i++)
        {
            if(KeyNotes[i].start || KeyNotes[i].uid)
            {
                vtxidx = 1;
                break;
            }
        }
        
        if(vtxidx)
            continue;
        #endif
        
        if(midisize)
        {
            midisize >>= 1;
            continue;
        }
        #endif
        
        puts("MIDI end");
        
        glFinish();
        free(quads);
        
        while(notelist)
        {
            NoteNode* __restrict mn = notelist->next;
            free(notelist);
            notelist = mn;
        }
        
        break;
        //#endif
    }
    
    ded:
    
    wglMakeCurrent(0, 0);
    
#if defined(TIMI_CAPTURE) && !defined(TIMI_NOCAPTURE)
    CloseHandle(hpipe);
    PostQuitMessage(1);
    CloseWindow(glwnd);
#endif
    
    
    puts("Renderer died");
    
    return 0;
}
