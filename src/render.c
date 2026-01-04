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
static const u32 keymul = 8;
#else
static const u32 keymul = 1;
#endif

#ifdef TIMI_CAPTURE
const MMTick FPS_DENIM = TIMI_FPS_DENIM;//625*4;//28125;//3840;
const MMTick FPS_NOMIN = TIMI_FPS_NOMIN;//100;//130;
MMTick FPS_frame = 0;
volatile bool FPS_capture = false;

#ifdef TIMI_CUSTOMSCROLL
MMTick FPS_scroll = 0;
#endif
#endif


const float minheight = (1.0F / 256.0F)
#ifdef KEYBOARD
 * 1.5
#endif
;

#if defined(HDR) && !defined(FASTHDR)
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
#endif

#if !defined(GLTEXT) && (defined(TEXTNPS))
#error This feature combination requires GLTEXT
#endif

#ifdef DYNASCROLL

#define TICKVAL syncvalue
#define TICKVAR mmnotesync

#define tickheight 40000

const float tickscale = 1.0F / (float)tickheight;
const u32 minwidth = tickheight / 32;

#else
const u32 minwidth = 16;
#ifdef TIMI_CUSTOMSCROLL
#define TICKVAL FPS_scroll
#else
#define TICKVAL PlayerReal->TickCounter
#endif
#define TICKVAR PlayerNotecatcher->TickCounter
#endif



extern HWND glwnd;
extern HDC dc;
extern HGLRC glctx;
extern HANDLE vsyncevent;
extern RECT erect;

extern BOOL canrender;

extern HMODULE KSModule;


extern size_t midisize;

#ifdef TEXTNPS
struct histogram* hist;
u64 notecounter;
#endif



u64 drawnotesraw = 0;
u64 drawnotes = 0;

static void WINAPI DebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if(type == GL_DEBUG_TYPE_ERROR)
    printf("GL ERROR %i | source = 0x%X, type = 0x%X, severity = 0x%X, message = %s\n",
        id, source, type, severity, message);
}


struct NoteNode
{
    struct NoteNode* next;
#ifndef OVERLAPREMOVE
    struct NoteNode* overlapped_voce; // Either forwards or backwards, depending on layering
#endif
    MMTick start;
    MMTick end;
    u32 uid;
    u32 layering;
};
typedef struct NoteNode NoteNode;

struct ActiveNode
{
    NoteNode* NoteTop;
#if !defined(OVERLAPREMOVE) && !defined(PFALAYER)
    NoteNode* NoteBottom;
#endif
#ifdef OVERLAPREMOVE
#ifdef DENSEMERGE
    NoteNode* Dense;
#endif
    u32 Layering;
    MMTick LastStartTime;
#endif
};

struct LineSettings
{
    KCOLOR NoteCornersJelly[4];
    KCOLOR NoteCornersCrust[4];
    KCOLOR BaseColor;
};

const size_t szNode = sizeof(NoteNode);

MMPlayer* PlayerNotecatcher;
MMPlayer* PlayerReal;

struct quad* quads;
size_t vtxidx;
const size_t vertexsize =
#ifdef _M_IX86
    1 << 12;
#else
    //1 << 18;
    //1 << 12;
    1 << 18;
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

static struct ActiveNode* ActiveNoteList;
static struct LineSettings* LineTable;
static struct LineSettings LineKeyWhite;
static struct LineSettings LineKeyBlack;
static struct LineSettings LineBar;

size_t notealloccount;
size_t currnotealloc;

static KCOLOR color_blacken1(KCOLOR color)
{
#ifndef HDR
    return
    (
        ((color & 0xFEFEFE) >> 1)
        +
        ((color & 0xF0F0F0) >> 4)
    ) | (color & 0xFF000000);
#else
    return (KCOLOR)
    {
        color.r * 0.625F,
        color.g * 0.625F,
        color.b * 0.625F,
        color.a,
    };
#endif
}

static KCOLOR color_blacken2(KCOLOR color)
{
#ifndef HDR
    return ((color & 0xFCFCFC) >> 2) | (color & 0xFF000000);
#else
    return (KCOLOR)
    {
        color.r * 0.25F,
        color.g * 0.25F,
        color.b * 0.25F,
        color.a,
    };
#endif
}

static KCOLOR color_FromNative(DWORD color)
{
#ifdef HDR
    return (KCOLOR)
    {
        (BYTE)(color >> 0) / 255.0F,
        (BYTE)(color >> 8) / 255.0F,
        (BYTE)(color >> 16) / 255.0F,
        (BYTE)(color >> 24) / 255.0F
    };
#else
    return color;
#endif
}

static KCOLOR color_FromRGB(DWORD color)
{
    color = 0
        | (((color >> 16) & 0xFF) <<  0)
        | (((color >>  8) & 0xFF) <<  8)
        | (((color >>  0) & 0xFF) << 16)
        | (color & 0xFF000000);
    
    if(!(color & 0xFF000000))
        color |= 0xFF000000;
    
    return color_FromNative(color);
}

static void LineInstall3(struct LineSettings* __restrict line, KCOLOR color0, KCOLOR color1, KCOLOR crust)
{
    #if defined(MMVKEY)
    line->NoteCornersJelly[0] = color1;
    line->NoteCornersJelly[1] = color0;
    line->NoteCornersJelly[2] = color0;
    line->NoteCornersJelly[3] = color1;
    #elif defined(PFAKEY)
    line->NoteCornersJelly[0] = color0;
    line->NoteCornersJelly[1] = color0;
    line->NoteCornersJelly[2] = color1;
    line->NoteCornersJelly[3] = color1;
    #else
    line->NoteCornersJelly[0] = color0;
    line->NoteCornersJelly[1] = color0;
    line->NoteCornersJelly[2] = color0;
    line->NoteCornersJelly[3] = color0;
    #endif
    
    line->NoteCornersCrust[0] = crust;
    line->NoteCornersCrust[1] = crust;
    line->NoteCornersCrust[2] = crust;
    line->NoteCornersCrust[3] = crust;
    
    line->BaseColor = color0;
}

static void LineInstall2(struct LineSettings* __restrict line, KCOLOR color0, KCOLOR color1)
{
    LineInstall3(line, color0, color1, color_blacken2(color0));
}

static void LineInstall1(struct LineSettings* __restrict line, KCOLOR color)
{
    LineInstall2(line, color, color_blacken1(color));
}

static KCOLOR GetPianoColor(bool blackflag)
{
    return blackflag
    
    #ifndef HDR
    #ifdef GLOW
    ? 0xFF111111 : 0xFF808080;
    #else
    ? 0xFF202020 : 0xFFF0F0F0;
    #endif
    #else
        #ifndef TRIPPY
        ? (KCOLOR){ 0.04296875F, 0.04296875F, 0.04296875F, 1.0F }
        : (KCOLOR){ 0.25F, 0.25F, 0.25F, 1.0F };
        #else
        ? (KCOLOR){ 0.06640625F, 0.06640625F, 0.06640625F, 1.0F }
        : (KCOLOR){ 0.5F, 0.5F, 0.5F, 1.0F };
        #endif
    #endif
}


//seems to work properly
static void NoteAppend(NoteNode* __restrict node)
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

static NoteNode* __restrict NoteAlloc()
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

static void NoteFree(NoteNode* __restrict node)
{
    if(__builtin_expect(!freelist_head, 0)) //keep track of first element for O(1) return
    {
        //if(freelist)
        //    puts("Oh no, returnhead it set after freelist has been set");
        freelist_head = node; //TODO: let's hope freelist will always be null :/
    }
    
    node->next = freelist;
    freelist = node;
}

#ifdef DYNASCROLL
static MMTick syncvalue;

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
u32 histlast = 0;
u32 histwrite = 0;
MMTick histsum = 0;
MMTick histdelay = 0;
MMTick onehztimer = 0;
u64 currnote = 0;
u64 currnps = 0;

int(WINAPI*kNPOriginal)(DWORD msg);
void(WINAPI*kNSOriginal)(MMPlayer* syncplayer, DWORD dwDelta);
static int WINAPI kNPIntercept(DWORD note)
{
    if((((BYTE)note & 0xF0) == 0x90))
    {
        if((BYTE)(note >> 16))
            currnote++;
    }
    
    //if((BYTE)note >= 0xA0)
    if(0)
    {
        printf("Event @ %5u: %8llu: %02X", PlayerNotecatcher->CurrentTrack->trackid, PlayerNotecatcher->TickCounter, (u8)note);
        
        switch((BYTE)note)
        {
            case 0xA0 ... 0xAF:
                printf(" %02X %02X (Key Aftertouch)\n", (u8)(note >> 8), (u8)(note >> 16));
                break;
            
            case 0xB0 ... 0xBF:
                printf(" %02X %02X (CC %3u = %3u)\n", (u8)(note >> 8), (u8)(note >> 16), (u8)(note >> 8), (u8)(note >> 16));
                break;
            
            case 0xC0 ... 0xCF:
                printf(" %02X    (Instrument select)\n", (u8)(note >> 8));
                break;
            
            case 0xD0 ... 0xDF:
                printf(" %02X    (Chn Aftertouch)\n", (u8)(note >> 8));
                break;
            
            case 0xE0 ... 0xEF:
            {
                DWORD bend = ((note >> 8) & 0x7F) + (((note >> 16) & 0x7F) << 7);
                
                printf(" %02X %02X (Pitchbend %+6i)\n", (u8)(note >> 8), (u8)(note >> 16), (int)bend - 8192);
                break;
            }
            
            default:
                printf(" %02X %02X (junk)\n", (u8)(note >> 8), (u8)(note >> 16));
                break;
        }
    }
    
    if(kNPOriginal)
        return kNPOriginal(note);
    else
        return 0;
}

static void WINAPI kNPSync(MMPlayer* syncplayer, DWORD dwDelta)
{
    struct histogram* __restrict hhist = hist + (histwrite++);
    if(histwrite == (DWORD)1e7)
        histwrite = 0;
    
    hhist->delta = dwDelta * syncplayer->tempo * 10 / syncplayer->timediv;
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
static MMTick mmnotesync;
#endif

static void WINAPI NoteReturn(MMPlayer* syncplayer, DWORD dwDelta)
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

static __attribute__((noinline)) void FlushToilet()
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

static int WINAPI dwLongMsg(DWORD dwMsg, LPCVOID ptr, DWORD len)
{
    if(len > 256)
    {
        printf("LongMsg too long (%i), ignoring\n", len);
        return 1;
    }
    
    if(KSModule && (BYTE)dwMsg == 0xF0)
    {
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
    }
    
    //printf("LongMsg: %8X %u\n", dwMsg, len);
    
    dwMsg &= 0xFFFF;
    
    if(((dwMsg == 0x7FFF) || (dwMsg == 0x0AFF)) && len >= 8) // undocumented color meta
    {
        return 0;
        
    #if 0
        LPBYTE data = (LPBYTE)ptr;
        if(data[0] != 0x00 || data[1] != 0x0F) // extended meta
            return 0;
        
        // Don't remember what data[3] does :(
        
        BYTE ch = data[2];
        DWORD i, j;
        
        if(ch >= 16)
        {
            i = 0;
            j = 16 * 2;
        }
        else
        {
            i = ch * 2;
            j = i + 2;
        }
        
        const struct LineSettings* __restrict line = &LineTable[PlayerReal->CurrentTrack->trackid * (16 << 1)];
        
        KCOLOR color1, color2;
        
    #ifndef HDR
        color1 = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    #else
        color1 = (KCOLOR)
        {
            data[4] * (1.0F / 255.0F),
            data[5] * (1.0F / 255.0F),
            data[6] * (1.0F / 255.0F),
            data[7] * (1.0F / 255.0F),
        };
    #endif
        
        if(len >= 12)
        {
        #ifndef HDR 
            color2 = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
        #else
            color2 = (KCOLOR)
            {
                data[ 8] * (1.0F / 255.0F),
                data[ 9] * (1.0F / 255.0F),
                data[10] * (1.0F / 255.0F),
                data[11] * (1.0F / 255.0F),
            };
        #endif
        }
        else
        {
        #ifdef PFAKEY
            color2 = color_blacken1(color1);
        #else
            color2 = color1;
        #endif
        }
        
        do
        {
            colorbase[i+0] = color1;
            colorbase[i+1] = color2;
            
            i += 2;
        }
        while(i < j);
    #endif
    }
    
    
    return 0;
}

#ifdef TIMI_CAPTURE
static int WINAPI FPS_ShortMsg(DWORD note)
{
    return 0;
}

#ifdef TIMI_IMPRECISE
#define NEXTFRAME ((10000000ULL / FPS_DENIM) * PlayerReal->timediv * FPS_frame * FPS_NOMIN)
#else
#define NEXTFRAME ((10000000ULL * FPS_frame * FPS_NOMIN * PlayerReal->timediv) / FPS_DENIM)
#endif


static void FPS_SyncFunc(MMPlayer* syncplayer, DWORD dwDelta)
{
    ULONGLONG nextframe = NEXTFRAME;
    if(nextframe <= PlayerReal->RealTimeUndiv)
    {
        #ifdef TIMI_CUSTOMSCROLL
        FPS_scroll = nextframe;
        #endif
        
        int(WINAPI*NtDelayExecution)(BOOL doalert, INT64* timeptr)
                = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtDelayExecution");
        
        while(!PlayerNotecatcher->done && (INT64)(PlayerNotecatcher->RealTime - (PlayerReal->RealTime + PlayerNotecatcher->SyncOffset)) < 0)
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
            if(nextframe <= PlayerReal->RealTimeUndiv)
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
/*
static int WINAPI LongMessage(DWORD dwMsg, LPCVOID ptr, DWORD len)
{
    return 0;
}*/

/*
    Callback for the player
    
    Make sure to keep this function lower cycle count than
    KShortMsg, otherwise the note catcher will severely lag behind.
    
    Currently it's slighly less than KShortMsg, but if
    the malloc hits then it'll be much, much slower.
*/
static int WINAPI dwEventCallback(DWORD note)
{
    if(COMPILER_UNLIKELY((u8)note >= 0xA0))
        return 0;
    
    u32 uid = 0
        | (u8)(note >> 8)
        | ((note & 0xF) << 8)
        | (PlayerNotecatcher->CurrentTrack->trackid << 12)
    ;
    
    MMTick curr = TICKVAR;
    
    struct ActiveNode* __restrict const active = &ActiveNoteList[uid];
    
    NoteNode* __restrict node;
    
    if((note & 0x10) && ((note >> 16) & 0xFF)) // NoteOn
    {
        NoteNode* __restrict backupnode = active->NoteTop;
        
    #ifdef OVERLAPREMOVE
        ++(active->Layering);
        
        // When overlap remove, eat spam, or split note
        //  to keep layer depth at 1, minimizing overdraw
        if(backupnode)
        {
            if(active->LastStartTime == curr)
            {
                //HACK: restarting notes is super risky
                backupnode->end = ~0;
                return 0;
            }
        #ifdef DENSEMERGE
            else if((active->LastStartTime + 1) == curr)
            {
                active->LastStartTime = curr;
                
                backupnode->end = ~0;
                return 0;
            }
        #endif
            
            if(!~backupnode->end)
                backupnode->end = curr;
        }
        
        active->LastStartTime = curr;
    #endif
        
        node = NoteAlloc();
        if(!node) //allocation failed
            return 1;
        
    #ifndef OVERLAPREMOVE
        // PFA stacking like pyramid layers.
        // MIDI is stacking FIFO / Queue.
        
        #ifdef PFALAYER
            node->overlapped_voce = backupnode;
        #else
            node->overlapped_voce = 0;
            
            if(backupnode)
                backupnode->overlapped_voce = node;
            else
                active->NoteBottom = node;
        #endif
    #endif
        
        active->NoteTop = node;
        
        node->start = curr;
        node->end = ~0;
        node->uid = uid;
        
        NoteAppend(node);
        
        return 0;
    }
    
#if !defined(OVERLAPREMOVE) && !defined(PFALAYER)
    node = active->NoteBottom;
#else
    node = active->NoteTop;
#endif
    
    if(node)
    {
    #ifdef OVERLAPREMOVE
        if(active->Layering)
        {
            if(--(active->Layering))
                return 0;
        }
    #endif
        
        if(!~node->end)
            node->end = curr;
        
    #ifndef OVERLAPREMOVE
        node = node->overlapped_voce;
        
        #ifdef PFALAYER
            active->NoteTop = node;
        #else
            active->NoteBottom = node;
            if(!node)
                active->NoteTop = 0;
        #endif
    #endif
    }
    
    return 0;
}

static __attribute__((noinline)) void AddRawVtx(float offsy, float offst, float offsx, float offsr, const struct LineSettings* line)
{
    if(COMPILER_UNLIKELY(vtxidx >= vertexsize))
        FlushToilet();
    
    struct quad* __restrict ck = quads + (vtxidx++);
    
    float origoffsy = offsy;
    float origoffst = offst;
    float origoffsx = offsx;
    float origoffsr = offsr;
    
    #ifdef OUTLINE
    
    const float widthmagic =
        #ifdef PIANOKEYS
            150.0F * keymul / 1280.0F
        #else
            128.0F * keymul / 1280.0F
        #endif
        ;
    
    #ifdef OLDDENSE
    origoffsy += minheight;
    origoffst -= minheight;
    origoffsx += widthmagic;
    origoffsr -= widthmagic;
    #else
    offsy -= minheight;
    offst += minheight;
    offsx -= widthmagic;
    offsr += widthmagic;
    #endif
    
    ck->quads[0] = (struct quadpart){offsx, offst, line->NoteCornersCrust[0]};
    ck->quads[1] = (struct quadpart){offsx, offsy, line->NoteCornersCrust[1]};
    ck->quads[2] = (struct quadpart){offsr, offsy, line->NoteCornersCrust[2]};
    ck->quads[3] = (struct quadpart){offsr, offst, line->NoteCornersCrust[3]};
    
    if((origoffst - origoffsy) < minheight)
        return;
    
    if(COMPILER_UNLIKELY(vtxidx >= vertexsize))
        FlushToilet();
        
    ck = quads + (vtxidx++);
    #endif
    
    ck->quads[0] = (struct quadpart){origoffsx, origoffst, line->NoteCornersJelly[0]};
    ck->quads[1] = (struct quadpart){origoffsx, origoffsy, line->NoteCornersJelly[1]};
    ck->quads[2] = (struct quadpart){origoffsr, origoffsy, line->NoteCornersJelly[2]};
    ck->quads[3] = (struct quadpart){origoffsr, origoffst, line->NoteCornersJelly[3]};
}

#ifdef PIANOKEYS
static const u8 keyoffssesses[] =
{
    0, 4, 8, 14, 16, 24, 28, 32, 37, 40, 46, 48
};

static void pianokey(float* __restrict offsx, float* __restrict offsr, u8 num)
{
    u32 div = (u32)num / 12;
    u32 mod = (u32)num % 12;
    
    num = keyoffssesses[mod];
    
    if(mod > 4)
        mod++;
    
    DWORD rawoffs = (div * 7 * keymul) + num;
    
    *offsx = rawoffs;
    //*offsr = rawoffs + ((mod & 1) ? (keymul - (keymul >> 2)) : keymul);
    *offsr = (float)(rawoffs + (keymul - ((keymul >> 2) * (mod & 1))));
}
#endif

static __attribute__((noinline)) void AddVtx(const NoteNode* __restrict localnode, MMTick currtick, float tickscale)
{
    #ifdef PIANOKEYS
    float offsx;
    float offsr;
    pianokey(&offsx, &offsr, localnode->uid);
    #else
    DWORD rawoffs = localnode->uid & 0xFF;
    float offsx = rawoffs;
    float offsr = rawoffs + 1;
    #endif
    
    float offsy = -1.0F;
    //if(localnode.start > currtick)
        offsy = ((float)((int)(localnode->start - currtick)) * tickscale) - 1.0F;
    float offst = 1.0F;
    if(~localnode->end)
    {
        offst = ((float)((int)(localnode->end - currtick)) * tickscale) - 1.0F;
    }
    
    AddRawVtx(offsy, offst, offsx, offsr, &LineTable[localnode->uid >> 8]);
}

#ifdef DEBUGTEXT
static __attribute__((noinline)) void AddWideVtx(MMTick start, float height, MMTick currtick, float tickscale, DWORD range, DWORD color)
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
    
    KCOLOR kc_send[2];
    kc_send[0] = kc;
    kc_send[1] = kc;
    
    AddRawVtx(offsy, offst, offsx, offsr, kc_send);
}
#endif

#if defined(PFACOLOR)
static __attribute__((noinline)) DWORD HSV2RGB(float hue, float saturation, float value)
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
    
    DWORD dwColor = 0;
    dwColor |= (BYTE)(((r + m) * 255.0F) + 0.5F) << 0;
    dwColor |= (BYTE)(((g + m) * 255.0F) + 0.5F) << 8;
    dwColor |= (BYTE)(((b + m) * 255.0F) + 0.5F) << 16;
    return dwColor;
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

static __attribute__((noinline)) KCOLOR LerpColor(KCOLOR color, KCOLOR def, float a)
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

static __attribute__((noinline)) void LerpTable(struct LineSettings* __restrict target, const struct LineSettings* __restrict source, float a)
{
#define h(v) target->v = LerpColor(target->v, source->v, a)
    h(NoteCornersJelly[0]);
    h(NoteCornersJelly[1]);
    h(NoteCornersJelly[2]);
    h(NoteCornersJelly[3]);
    h(NoteCornersCrust[0]);
    h(NoteCornersCrust[1]);
    h(NoteCornersCrust[2]);
    h(NoteCornersCrust[3]);
#undef h
}

#define h(v) target->v = LerpColor(target->v, source, a)
static __attribute__((noinline)) void LerpTableSingleJelly(struct LineSettings* __restrict target, KCOLOR source, float a)
{
    h(NoteCornersJelly[0]);
    h(NoteCornersJelly[1]);
    h(NoteCornersJelly[2]);
    h(NoteCornersJelly[3]);
}
static __attribute__((noinline)) void LerpTableSingleCrust(struct LineSettings* __restrict target, KCOLOR source, float a)
{
    h(NoteCornersCrust[0]);
    h(NoteCornersCrust[1]);
    h(NoteCornersCrust[2]);
    h(NoteCornersCrust[3]);
}
static __attribute__((noinline)) void LerpTableSingle(struct LineSettings* __restrict target, KCOLOR source, float a)
{
    h(NoteCornersJelly[0]);
    h(NoteCornersJelly[1]);
    h(NoteCornersJelly[2]);
    h(NoteCornersJelly[3]);
    h(NoteCornersCrust[0]);
    h(NoteCornersCrust[1]);
    h(NoteCornersCrust[2]);
    h(NoteCornersCrust[3]);
}
#undef h

#endif

static DWORD trackcount;

#ifdef WMA_SIZE
ULONGLONG* fps_wma;
DWORD fps_wmai;
#endif

static BOOL isrender;

#ifdef HEADLESS
static
#endif
DWORD WINAPI RenderThread(PVOID lpParameter)
{
    puts("Hello from renderer");
    
    isrender = FALSE;
    
    wglMakeCurrent(dc, glctx);
    
    //if(glDebugMessageCallbackARB && !IsBadCodePtr(glDebugMessageCallbackARB))
    {
        //glEnable(GL_DEBUG_OUTPUT);
        //glDebugMessageCallbackARB(DebugCB, NULL);
    }
    
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
    
    /*
    GLuint cap_db = 0;
    glGenRenderbuffers(1, &cap_db);
    glBindRenderbuffer(GL_RENDERBUFFER, cap_db);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
        capsize.x, capsize.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cap_db);
    
    glBindRenderbuffer(GL_RENDERBUFFER, cap_rb);
    */
    
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
    
    PlayerNotecatcher = ((MMPlayer**)lpParameter)[0];
    PlayerReal = ((MMPlayer**)lpParameter)[1];
    
    midisize += 2 * sizeof(MMPlayer);
    trackcount = PlayerNotecatcher->TrackCount;
    trackcount *= 16; // 16 channels per track
    
    LineTable = malloc(trackcount * sizeof(*LineTable));
    if(!LineTable)
        puts("No colortable, fuck");
    
    midisize += sizeof(*LineTable) * trackcount;
    
    do
    {
        DWORD seed = 1;
        
    #ifdef PFACOLOR
        seed = 56;
    #endif
        
        DWORD i = 0;
        
        while(i < trackcount)
        {
            DWORD ic = 3;
            
            DWORD col = 0xFF;
            
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
            col = dominodark[(((i >> 5) + 6) % (sizeof(dominodark)/sizeof(*dominodark)))];
            
            col = (((col >>  0) & 0xFF) << 16)
                | (((col >>  8) & 0xFF) <<  8)
                | (((col >> 16) & 0xFF) <<  0)
                | (0xFF << 24);
            
            ic = 16;
    #else
            ic = 1;
    #ifdef PFACOLOR
            float light = ((seeds[0] % 20) + 80) / 100.0F;
            float sat = ((seeds[1] % 40) + 60) / 100.0F;
            float hue = (seeds[2] % 360) / 360.0F;
            col = HSV2RGB(hue, sat, light)
            #ifndef HDR
                | (0xFF << 24)
            #endif
            ;
    #endif
    #endif
            
            do
            {
                KCOLOR real_color;
            #ifndef HDR
                real_color = col;
            #else
                real_color = (KCOLOR){
                    (BYTE)(col >> 0) * (1.0F / 255.0F),
                    (BYTE)(col >> 8) * (1.0F / 255.0F),
                    (BYTE)(col >> 16) * (1.0F / 255.0F),
                    1.0F
                };
            #endif
                
                struct LineSettings* __restrict line = &LineTable[i++];
                
                LineInstall1(line, real_color);
            }
            while(--ic);
        }
        
        {
            KCOLOR tmp;
            
            tmp = GetPianoColor(0);
            LineInstall3(&LineKeyWhite, tmp, color_blacken1(tmp), color_FromNative(0xFF404040));
            LineKeyWhite.NoteCornersJelly[0] = color_blacken1(tmp);
            LineKeyWhite.NoteCornersJelly[1] = tmp;
            LineKeyWhite.NoteCornersJelly[2] = tmp;
            LineKeyWhite.NoteCornersJelly[3] = LineKeyWhite.NoteCornersJelly[0];
            
            tmp = GetPianoColor(1);
            LineInstall3(&LineKeyBlack, tmp, tmp, color_blacken1(tmp));
            ARRAY_LVALUE(LineKeyWhite.NoteCornersCrust, LineKeyBlack.NoteCornersJelly);
            LineKeyBlack.NoteCornersJelly[0] = color_FromNative(0xC0242424);
            LineKeyBlack.NoteCornersJelly[1] = tmp;
            LineKeyBlack.NoteCornersJelly[2] = color_FromNative(0xFF242424);
            LineKeyBlack.NoteCornersJelly[3] = tmp;
            
            LineInstall1(&LineBar, color_FromRGB(0xFFAB2330));
        }
    }
    while(0);
    
    trackcount *= 256; // 256keys per channel per track
    
    ActiveNoteList = malloc(sizeof(*ActiveNoteList) * trackcount);
    if(!ActiveNoteList)
        puts("No ActiveNoteList, fuck");
    ZeroMemory(ActiveNoteList, sizeof(*ActiveNoteList) * trackcount);
    
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
                #if 1
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
    
    PlayerNotecatcher->SyncPtr = &PlayerReal->RealTime;
    PlayerNotecatcher->SyncOffset = 50000000;
    PlayerNotecatcher->KShortMsg = dwEventCallback;
    PlayerNotecatcher->KLongMsg = 0;//LongMessage;
    PlayerNotecatcher->KSyncFunc = NoteReturn;
    
    PlayerReal->SleepTicksMax = 1;
    PlayerReal->KLongMsg = dwLongMsg;
    
    
    #ifdef TIMI_CAPTURE
    ULONGLONG timersync = ~0;
    timersync = timersync >> 2;
    
    #ifndef TIMI_NOCAPTURE
    PlayerReal->SyncPtr = (LONGLONG*)&timersync;
    PlayerReal->SyncOffset = 0;
    #endif
    
    #ifndef TIMI_NOWAIT
    #ifdef TIMI_SILENT
    PlayerReal->KShortMsg = FPS_ShortMsg;
    #endif
    PlayerReal->KSyncFunc = FPS_SyncFunc;
    #endif
    
    PlayerNotecatcher->SleepTicks = 1;
    PlayerNotecatcher->SleepTimeMax = 0;
    #endif
    
    #if defined(TEXTNPS)
    kNPOriginal = PlayerReal->KShortMsg;
    kNSOriginal = PlayerReal->KSyncFunc;
    PlayerReal->KShortMsg = kNPIntercept;
    PlayerReal->KSyncFunc = kNPSync;
    #elif defined(DYNASCROLL)
    PlayerReal->KSyncFunc = NoteSync;
    
    syncvalue = 0;
    mmnotesync = 0;
    #endif
    
    #ifdef TIMI_CAPTURE
    if(!PlayerReal->KShortMsg)
        PlayerReal->KShortMsg = FPS_ShortMsg;
    #endif
    
    CreateThread(0, 0x4000, PlayerThread, PlayerNotecatcher, 0, 0);
    //CreateThread(0, 0x4000, PlayerThread, PlayerReal, 0, 0);
    
    extern int _VDSO_QueryInterruptTime(PULONGLONG _outtime);
    //int(WINAPI*NtQuerySystemTime)(ULONGLONG* timeptr) = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtQuerySystemTime");
    int(WINAPI*NtQuerySystemTime)(PULONGLONG timeptr) = _VDSO_QueryInterruptTime;
    
    ULONGLONG prevtime;
    NtQuerySystemTime(&prevtime);
    ULONGLONG currtime = prevtime;
    
    DWORD timeout = 0;
    
    //glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LEQUAL);
    
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
            CreateThread(0, 0x4000, PlayerThread, PlayerReal, 0, 0);
            
            isrender = TRUE;
            
            #if defined(TIMI_CAPTURE) && !defined(TIMI_NOCAPTURE)
            
            PlayerReal->SyncPtr = (LONGLONG*)&timersync;
            PlayerReal->SyncOffset = 0;
            
            //PlayerReal->KShortMsg = FPS_ShortMsg;
            //PlayerReal->KSyncFunc = FPS_SyncFunc;
            #endif
        }
        
        #if defined(TIMI_CAPTURE) && !defined(TIMI_NOWAIT)
        if(!FPS_capture && !PlayerReal->done)
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
        ULONGLONG tickheight =
        #ifdef CUSTOMTICK
        CUSTOMTICK
        ;
        #else
        (250000ull) * PlayerReal->timediv / (PlayerReal->tempo);
        if(!tickheight)
            tickheight = 1;
        #endif
        ULONGLONG notesync = TICKVAR;
        ULONGLONG currtick = TICKVAL;
        float tickscale = 1.0F / (float)tickheight;
        #endif
        
        #ifdef KEYBOARD
        currtimer = PlayerReal->RealTime;
        #endif
        
        if(currtick && (currtick >= PlayerNotecatcher->TickCounter))
            currtick = PlayerNotecatcher->TickCounter - 1;
        
        #ifdef DYNASCROLL
        BIND_IF(glUniform1f, uniGrTime, (float)(currtick / (double)tickheight * 0.25));
        #else
        BIND_IF(glUniform1f, uniGrTime, (float)((double)(PlayerReal->RealTime) / 1e7));
        #endif
        
        ULONGLONG midtick = currtick + tickheight;
        ULONGLONG toptick = midtick + tickheight;
        
        
        //printf("%10llu %10llu %i %u\n", notesync, currtick, (int)(notesync - currtick), PlayerReal->tempo);
        
        // ===[BAD IDEA NEVER UNCOMMENT THIS]===
        /*if(!timeout && notesync < midtick)
        {
            midtick = notesync;
            toptick = midtick + tickheight;
            currtick = midtick - tickheight;
        }*/
        // ===[BAD IDEA NEVER UNCOMMENT THIS]===
        
        #if defined(HDR) && !defined(FASTHDR)
        #warning Fix this soon
        #if 0
        #error Fix this soon
        #ifdef TRIPPY
        const float uc = 0.1F;
        const float kc = -1.0F;
        #else
        const float uc = 0.0F;
        const float kc = -1.0F;
        #endif
        #if 0 /* TODO: backfill */
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
        #endif
        
        
        if(vtxidx)
            FlushToilet();
        
        BIND_IF(glUniform1f, attrGrNotemix, 1.0F);
        
        #if 0 && defined(DEBUGTEXT) /* beat visualizer fills notes per beat */
        {
            LONGLONG asdtick = currtick - (currtick % PlayerReal->timediv) - PlayerReal->timediv - PlayerReal->timediv;
            do
            {
                asdtick += PlayerReal->timediv;
                
                for(int asdi = 0; asdi != 0x80; asdi++)
                {
                    NoteNode debugnode;
                    debugnode.next = 0;
                    debugnode.uid = asdi;
                    debugnode.start = asdtick;
                    debugnode.end = asdtick + PlayerReal->timediv;
                    
                    AddVtx(&debugnode, currtick, tickscale);
                    
                    //AddWideVtx(currtick, 2.0F, currtick, tickscale, asdi | (asdi << 8), 0xFF7F7F7F);
                }
                
                AddWideVtx((ULONGLONG)asdtick, 1.0F / 64.0F, currtick, tickscale, 0x7F, -1);
                
            }
            while((LONGLONG)(asdtick - toptick) < 0);
            
            if(vtxidx)
                FlushToilet();
        }
        #endif
        
        NoteNode* __restrict prevnote = 0;
        NoteNode* __restrict currnote = VisibleNoteList;
        
        //loop over freeable notes first
        while(currnote && currnote->start <= currtick) // already triggered
        {
        #ifdef KEYBOARD
            NoteNode* lmn = &KeyNotes[(BYTE)currnote->uid];
            
            if(lmn->uid != currnote->uid)
            {
                lmn->uid = currnote->uid;
                lmn->start = 1400000;
                //lmn->start = 2100000;
            }
            else if(lmn->start < 1000000)
                lmn->start = 1000000;
        #endif
            
            if(currnote->end > currtick) //note not finished yet
            {
                NoteNode* localnode = currnote;
                prevnote = currnote;
                currnote = localnode->next;
                
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
            NoteNode* localnode = currnote;
            prevnote = currnote;
            currnote = localnode->next;
            
            AddVtx(localnode, currtick, tickscale);
        }
        
        if(vtxidx)
            FlushToilet();
        
        #ifdef GLTEXT
        drawnotes = drawnotesraw + vtxidx;
        #endif
        
        #ifdef DEBUGTEXT
        BIND_IF(glUniform1f, attrGrNotemix, 0.0F);
        
        LONGLONG debugtick = currtick - (currtick % PlayerReal->timediv) - PlayerReal->timediv - PlayerReal->timediv;
        do
        {
            debugtick += PlayerReal->timediv;
            
            NoteNode debugnode;
            debugnode.next = 0;
            debugnode.uid = 0;
            debugnode.start = debugtick;
            debugnode.end = debugtick + (PlayerReal->timediv >> 1) + (PlayerReal->timediv >> 2);
            
            AddVtx(&debugnode, currtick, tickscale);
            
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
        
        #if defined(HDR)
        BIND_IF(glUniform1f, attrGrNotemix, 0.0F);
        //BIND_IF(glUniform1f, attrGrNotemix, 1.0F / 64.0F);
        #endif
        
        #if defined(PIANOBAR)
        #error Fix this soon
        AddRawVtx(-1.02, -0.98, 0, 600, (KCOLOR){1.0, 0.0, 0.0, 1});
        #endif
        
        ULONGLONG delta = 4000000;
        
        if((currtimer - lasttimer) < 250000) // 250ms
            //delta = (currtimer - lasttimer) * 8;
            delta = (currtimer - lasttimer) * 6;
            //delta = (currtimer - lasttimer) * 3;
        
        if(!delta)
            delta = 10000; // 10ms
        
        #ifdef PIANOKEYS
        #if 1
            AddRawVtx(
                -1.05F, -1.0F,
                0.0F,
                600.0F
                , &LineBar);
        #endif
        
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
            
            struct LineSettings dwColor = !blackflag ? LineKeyWhite : LineKeyBlack;
            float coloralpha = (float)lmn->start * 0.000001F;
            
            if(lmn->start)
            {
                
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
                    LerpTableSingle(&dwColor, LineKeyBlack.BaseColor, grayalpha);
                }
                #endif
                
                //LerpTable(&dwColor, &LineTable[lmn->uid >> 8], coloralpha);
                LerpTableSingleJelly(&dwColor, LineTable[lmn->uid >> 8].BaseColor, coloralpha * 0.50F);
                LerpTableSingleCrust(&dwColor, LineTable[lmn->uid >> 8].BaseColor, coloralpha * 0.25F);
                
                #ifdef HDR
                shalpha[i] = coloralpha;
                shcolor[i] = LineTable[lmn->uid >> 8].BaseColor;
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
            
            float offt =
            #if 1
                -1.04F
            #else
                -1.0F
            #endif
            ;
            float offy = blackflag ? -1.30F : -1.5F;
            float range = offy;
            
            float tall = (offy - offt);
            
            if(blackflag)
                offt += (coloralpha - 1.0F) * tall * 0.1F * 0.5F;
            offy += (coloralpha - 1.0F) * tall * 0.1F;
            
            if(!blackflag && (offy > range))
            {
                struct LineSettings underkey = dwColor;
                LerpTableSingle(&underkey, LineKeyBlack.BaseColor, 0.375F);
                
                AddRawVtx(
                range, offy,
                #ifdef PIANOKEYS
                    offsx, offsr
                #else
                    posflag, posflag + 1
                #endif
                    , &underkey);
            }
            
            //AddRawVtx(1.0F, !blackflag ? 1.15F : 1.25F, posflag, posflag + 2, &colortable[(KeyNotes[i].uid >> 8) << 1]); // ???
            AddRawVtx(
                offy, offt,
            #ifdef PIANOKEYS
                offsx, offsr
            #else
                posflag, posflag + 1
            #endif
                , &dwColor);
            
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
        //printf("Drawn: %10llu | Desync: %10lli\n", notesdrawn, PlayerReal->RealTime - PlayerNotecatcher->RealTime);
        //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        //glDrawArrays(GL_TRIANGLES, 0, 3);
        //glDrawElements(GL_TRIANGLES, 6 * vtxidx, GL_UNSIGNED_INT, indexes);
        
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
        if(!PlayerReal->done)
            continue;
        
        #ifdef GRACE
        #ifdef KEYBOARD
        vtxidx = 0;
        for(DWORD i = 0; i != 128; i++)
        {
            if(KeyNotes[i].start)
            {
                vtxidx = 1;
                break;
            }
        }
        
        if(vtxidx)
        {
            vtxidx = 0;
            continue;
        }
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
