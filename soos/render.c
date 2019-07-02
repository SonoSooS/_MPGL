#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GL/ctor.h"
#include "GL/core.h"
#include "GL/utility.h"

#include "player/mmplayer.h"


//#define TRACKID
#define PFACOLOR
#define PFAKEY
//#define ROUNDEDGE
//#define TRANSFORM
#define PIANOKEYS
//#define DYNASCROLL
//#define O3COLOR
#define OUTLINE
#define KEYBOARD
//#define TRIPPY
//#define WIDEMIDI
//#define TIMI_TIEMR
//#define ROTAT

const float minheight = (1.0F / 256.0F)
#ifdef KEYBOARD
 * 1.5
#endif
#ifdef TRANSFORM
/ 2
#endif
;


#if defined(ROUNDEDGE) && !defined(DYNASCROLL)
#define DYNASCROLL
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

#define TICKVAL player->TickCounter
#define TICKVAR ply->TickCounter
#endif



#ifdef ROUNDEDGE
#define NOTEVTX 36
#define QUADCOUNT 9
#else
#define NOTEVTX 6
#define QUADCOUNT 4
#endif

extern HWND glwnd;
extern HDC dc;
extern HGLRC glctx;
extern HANDLE vsyncevent;
extern RECT erect;

extern HMODULE KSModule;

GLuint sh;
GLint attrVertex, attrColor;

struct quadpart
{
    float x, y;
    GLuint color;
    GLuint padding;
};

struct quad
{
    struct quadpart quads[ QUADCOUNT ];
};

void PrintShaderInfo(GLuint shader)
{
    GLint loglen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);
    if(loglen > 0)
    {
        char ilog[256];
        GLsizei outloglen = 0;
        glGetShaderInfoLog(shader, 256, &outloglen, ilog);
        printf("%*.*s\n", outloglen, outloglen, ilog);
    }
    else
    {
        //puts("Unknown generic shader error");
    }
}

void CompileStandardShader()
{
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint psh = glCreateShader(GL_FRAGMENT_SHADER);
    
    
    const char* shadera =
        "#version 330 core\n"
        "in vec4 incolor;\n"
        "in vec2 inpos;\n"
    #ifndef PFAKEY
        "flat "
    #endif
        "out vec4 pcolor;\n"
    #ifdef TRANSFORM
        "out float zbuf;"
    #endif
    #ifdef ROUNDEDGE
        "out vec2 ppos;\n"
        "flat out vec2 fpos;\n"
    #endif
        "void main()\n"
        "{\n"
    #ifdef TRANSFORM
        "   vec2 rawpos = vec2(inpos.x, clamp(inpos.y, -1.25F, 1.0F));\n"
    #else
        "   vec2 rawpos = vec2(inpos.x, min(inpos.y, 1.0F));\n"
    #endif
    #ifdef WIDEMIDI
        "   rawpos.x = rawpos.x * 0.5F;\n"
    #endif
    #if defined(TRIPPY) && defined(TRANSFORM)
        "   pcolor = vec4(incolor.xyz * clamp((-rawpos.y - 0.95F) * 8.0F, 0.0F, 6.0F), incolor.w);\n"
    #elif defined(TRANSFORM)
        "   pcolor = vec4(incolor.xyz * clamp((-rawpos.y - 0.95F) * 32.0F, 1.0F, 6.0F), incolor.w);\n"
    #elif defined(TRIPPY)
        "   pcolor = vec4(incolor.xyz * clamp((-rawpos.y - 0.5F) * 4.0F, 0.0F, 5.6F), incolor.w);\n"
    #else
        "   pcolor = vec4(incolor.xyz * clamp((-rawpos.y - 0.865F) * 16.0F, 1.0F, 5.6F), incolor.w);\n"
    #endif
    #ifdef PIANOKEYS
        "   vec2 pos = vec2((rawpos.x - (150.0F * 0.5F)) * (2.0F / 150.0F), rawpos.y);\n"
    #else
        "   vec2 pos = vec2((rawpos.x - (128.0F * 0.5F)) * (2.0F / 128.0F), rawpos.y);\n"
    #endif
    #ifdef TRANSFORM
        "   float z = min((1.0F - pos.y) * 0.5F, 1.0F);\n"
        //"   vec2 vtxpos = vec2(pos.x * ((3.0F - pos.y) * 0.25F), (-z*z*2.0F) + 1.0F);\n"
        "   vec2 vtxpos = vec2(pos.x * ((3.0F - max(pos.y, -1.0F)) * 0.25F), (-pos.y*pos.y*2.0F) + 1.0F);\n"
        "   zbuf = z;\n"
    #else
        "   vec2 vtxpos = pos;\n"
    #endif
    #ifdef ROUNDEDGE
        "   ppos = vtxpos;\n"
        "   fpos = vtxpos;\n"
    #endif
    #ifdef KEYBOARD
    #ifdef ROTAT
        "   vtxpos.y = max((vtxpos.y * 0.8F) + 0.2F, -0.7F);\n"
    #else
        "   vtxpos.y = (vtxpos.y * 0.8F) + 0.2F;\n"
    #endif
    #endif
    #ifdef ROTAT
        "   const float PI = 3.1415926535897932384626433832795;\n"
        "   vtxpos = vec2(sin((vtxpos.x + 1.0F) * PI), cos((vtxpos.x + 1.0F) * PI) * (16.0F / 9.0F)) * (((vtxpos.y + 1.0F) * 0.75F) - 0.05F);\n"
    #endif
    #ifdef TRANSFORM
        "   gl_Position = vec4(vtxpos.xy, -z, 1.0F);\n"
    #else
        "   gl_Position = vec4(vtxpos.xy, 0.0F, 1.0F);\n"
    #endif
        "}\n"
        ;

    const char* shaderb =
        "#version 330 core\n"
    #ifndef PFAKEY
        "flat "
    #endif
        "in vec4 pcolor;\n"
    #ifdef TRANSFORM
        "in float zbuf;"
    #endif
    #ifdef ROUNDEDGE
        "in vec2 ppos;\n"
        "flat in vec2 fpos;\n"
    #endif
        "out vec4 outcolor;\n"
        "void main()\n"
        "{\n"
    #ifdef ROUNDEDGE
        "   vec2 rpos = ppos - fpos;\n"
        "   float af = (64.0F * abs(rpos.x*rpos.y)) + ((rpos.x*rpos.x)+(rpos.y*rpos.y));\n"
        "   float ua = af * 1024.0F;\n"
        "   float a = clamp(ua, 0.0F, 1.0F);\n"
        "   outcolor = vec4(pcolor.xyz, pcolor.w * a)"
        #ifdef TRANSFORM
            " * (0.2F + zbuf)"
        #endif
        ";\n"
        ""
    #else
        "   outcolor = vec4(pcolor.xyz"
        #ifdef TRANSFORM
            " * (0.2F + (zbuf*zbuf))"
        #endif
        ", pcolor.w);\n"
    #endif
        "}\n"
        ;
    GLint stat = 0;
    
    glShaderSource(vsh, 1, &shadera, 0);
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        PrintShaderInfo(vsh);
    
    glShaderSource(psh, 1, &shaderb, 0);
    glCompileShader(psh);
    glGetShaderiv(psh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        PrintShaderInfo(psh);
    
    sh = glCreateProgram();
    
    glAttachShader(sh, vsh);
    glAttachShader(sh, psh);
    
    glLinkProgram(sh);
    
    glGetProgramiv(sh, GL_LINK_STATUS, &stat);
    //if(stat != 1)
    {
        glGetProgramiv(sh, GL_INFO_LOG_LENGTH, &stat);
        if(stat > 0)
        {
            char ilog[256];
            GLsizei outloglen = 0;
            glGetProgramInfoLog(sh, 256, &outloglen, ilog);
            printf("%*.*s\n", outloglen, outloglen, ilog);
        }
        else
        {
            //puts("Unknown shader program error");
        }
        
    }
    
    attrVertex = glGetAttribLocation(sh, "inpos");
    attrColor = glGetAttribLocation(sh, "incolor");
    
    if(attrVertex < 0)
        puts("inpos not found");
    if(attrColor < 0)
        puts("incolor not found");
    
    glDetachShader(sh, psh);
    glDetachShader(sh, vsh);
    
    glDeleteShader(vsh);
    glDeleteShader(psh);
}

void WINAPI DebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if(type == GL_DEBUG_TYPE_ERROR)
    printf("GL ERROR %i | source = 0x%X, type = 0x%X, severity = 0x%X, message = %s\n",
        id, source, type, severity, message);
}


typedef struct NoteNode
{
    struct NoteNode* next;
    ULONGLONG start;
    ULONGLONG end;
    DWORD uid;
} NoteNode;

static MMPlayer* ply;
static MMPlayer* player;

static NoteNode* notelist;
static NoteNode* freelist;
static NoteNode* freelist_head;
static NoteNode* freelist_return;
static NoteNode* freelist_returnhead;

static NoteNode* VisibleNoteList;
static NoteNode* VisibleNoteListHead;

static NoteNode** ActiveNoteList;
static DWORD* colortable;

static QWORD notealloccount;

static struct quad* quads;
static size_t vertexsize;
static size_t vtxidx;


//seems to work properly
static inline void NoteAppend(NoteNode* node)
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

static inline NoteNode* NoteAlloc()
{
    if(notelist)
    {
        //pop from list
        
        NoteNode* ret = notelist; //pop last
        notelist = ret->next; //set list tail to next
        //ret->next = 0; //no need to set next to 0 because it'll be inserted into a list anyways which'll set this value
        return ret;
    }
    
    NoteNode* rn = (NoteNode*)malloc(sizeof(NoteNode));
    
    if(rn)
        notealloccount++;
    else
        puts("Out of Memory");
    
    return rn;
}

static inline void NoteFree(NoteNode* node)
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
    
    NoteNode* head = freelist_returnhead;
    freelist_returnhead = 0;
    NoteNode* f = freelist_return;
    freelist_return = 0;
    
    //append the current notelist to the end of the returnlist
    head->next = notelist;
    //set the current notelist to the returnlist head
    notelist = f;
}

#ifdef DYNASCROLL
static QWORD syncvalue;

static void NoteSync(MMPlayer* syncplayer, DWORD dwDelta)
{
    if(dwDelta)
    {
        DWORD dwValue = 60000000 / syncplayer->tempo;
        while(dwDelta--)
            syncvalue += dwValue;
    }
}
#endif

static inline void FlushToilet()
{
    if(vtxidx)
    {
        glBufferData(GL_ARRAY_BUFFER, vtxidx * sizeof(*quads),     0, GL_STREAM_DRAW);
        glBufferData(GL_ARRAY_BUFFER, vtxidx * sizeof(*quads), quads, GL_STREAM_DRAW);
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
    
    BYTE buf[257];
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
    
    DWORD uid = (BYTE)(note >> 8) | (note & 0xF) << 8
    #ifdef TRACKID
    | (ply->CurrentTrack->trackid << 12)
    #endif
    ;
    
    ULONGLONG curr = TICKVAR;
    
    NoteNode* node = ActiveNoteList[uid];
    
    if((note & 0x10) && (note >> 16) & 0xFF)
    {
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
        
        node = NoteAlloc();
        ActiveNoteList[uid] = node; //reset it no matter what
        
        if(!node) //allocation failed
            return 1;
        
        node->start = curr;
        node->end = ~0;
        node->uid = uid
    #ifndef TRACKID
        | (ply->CurrentTrack->trackid << 12)
    #endif
        ;
        
        NoteAppend(node);
        
        return 0;
    }
    
    if(node)
    {
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
    }
    
    return 0;
}

#ifdef PIANOKEYS
static inline int pianocode(BYTE num)
{
    int div = num / 12;
    int mod = num % 12;

    if(mod > 4)
        mod++;
    
    return ((div * 14) + mod);
}
#endif

static inline void AddRawVtx(float offsy, float offst, float offsx, float offsr, DWORD color1)
{
     if(vtxidx == vertexsize)
    {
        FlushToilet();
        //vtxidx = 0;
    }
    
    //offsx -= 0.125F;
    
    struct quad* ck = quads + (vtxidx++);
    
    #ifdef ROUNDEDGE
    //DWORD color = colortable[dwUID];
    //DWORD color1 = color; //(color & 0xFEFEFEFE) >> 1 | (1 << 31);
    DWORD color = color1;
    DWORD color2 = color1;
    DWORD color3 = color;
    
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
    
    //DWORD color1 = colortable[dwUID];
    
    #ifdef OUTLINE
    
    DWORD color3 = ((color1 & 0xFCFCFCFC) >> 2) | (0xFF << 24);
    
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
        ck->quads[0] = (struct quadpart){0, 0, 0};
        ck->quads[1] = (struct quadpart){0, 0, 0};
        ck->quads[2] = (struct quadpart){0, 0, 0};
        ck->quads[3] = (struct quadpart){0, 0, 0};
    }
    else
    {
        offsx += 0.25F;
        offsr -= 0.25F;
    #endif
        #ifdef PFAKEY
            DWORD color2 = ((color1 & 0xFEFEFEFE) >> 1) | (0xFF << 24);
        #else
            DWORD color2 = color1; //(color1 & 0xFEFEFEFE) >> 1;
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

static inline void AddVtx(NoteNode localnode, ULONGLONG currtick, float tickscale)
{
    #ifdef PIANOKEYS
    DWORD rawoffs = pianocode(localnode.uid/* & 0xFF*/);
    float offsx = rawoffs;
    float offsr = rawoffs + 2;
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
const DWORD dominodark[] =
{
    0x000078,
    0x004A78,
    0x007878,
    0x007828,
    0x647800,
    0x78003C,
    0x740078
};

const DWORD dominolight[] =
{
    0xBBBBFF,
    0x7FCDFF,
    0x61FFFF,
    0x7FFFA9,
    0xE9FF7F,
    0xFFC3E1,
    0xFCACFF
};
#endif

#ifdef KEYBOARD

static inline DWORD LerpColor(DWORD color, DWORD def, float a)
{
    //printf("LerpColor %f\n", a);
    
    float cr = (BYTE)color;
    float cg = (BYTE)(color >> 8);
    float cb = (BYTE)(color >> 16);
    
    float dr = (BYTE)def;
    float dg = (BYTE)(def >> 8);
    float db = (BYTE)(def >> 16);
    
    cr += (dr - cr) * a;
    cg += (dg - cg) * a;
    cb += (db - cb) * a;
    
    if(cr < 0) cr = 0; else if(cr > 255.49F) cr = 255.49F;
    if(cg < 0) cg = 0; else if(cg > 255.49F) cg = 255.49F;
    if(cb < 0) cb = 0; else if(cb > 255.49F) cb = 255.49F;
    
    return (BYTE)cr | ((DWORD)cg << 8) | ((DWORD)cb << 16) | (color & (0xFF << 24));
}

#endif

DWORD WINAPI RenderThread(PVOID lpParameter)
{
    puts("Hello from renderer");
    
    notelist = 0;
    freelist = 0;
    freelist_head = 0;
    freelist_return = 0;
    freelist_returnhead = 0;
    returnfailcount = 0;
    VisibleNoteList = 0;
    VisibleNoteListHead = 0;
    notealloccount = 0;
    
    ply = *(MMPlayer**)lpParameter;
    player = ((MMPlayer**)lpParameter)[1];
    
    DWORD trackcount = 0;
    
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
        col = dominodark[(((i >> 4) + 6) % 7)] | (0xFF << 24);
        ic = 16;
        while(ic--)
            colortable[i++] = col;
    #else
    #ifdef PFACOLOR
            float light = ((seeds[0] % 20) + 80) / 100.0F;
        #ifdef ROUNDEDGE
            float sat = ((seeds[1] % 64) + 36) / 100.0F;
        #else
            float sat = ((seeds[1] % 40) + 60) / 100.0F;
        #endif
            colortable[i++] = HSV2RGB((seeds[2] % 360) / 360.0F, sat, light) | (0xFF << 24);
    #else
            colortable[i++] = col;
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
    
    //UNUSED for now
    //trackcount >>= 12; // undo 16 * 256
    
    wglMakeCurrent(dc, glctx);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallbackARB(DebugCB, 0);
    
    uglSwapControl(1);
    
    glViewport(0, 0, 1280, 720);
    
    CompileStandardShader();
    
#ifdef _M_IX86
    vertexsize = 1 << 12;;
#else
    vertexsize = 1 << 18;
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
                    
                    indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 3;
                    indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 2;
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
        vertexsize >>= 1;
        if(!vertexsize)
            break;
    }
    
    if(!quads)
    {
        puts("Failed to callocate quads");
        goto ded;
    }
    
    if(glGenVertexArrays && glBindVertexArray)
    {
        GLuint g_vao;
        glGenVertexArrays(1, &g_vao);
        glBindVertexArray(g_vao);
    }
    
    GLuint g_vbo;
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    
    GLuint g_ebo;
    glGenBuffers(1, &g_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexsize * NOTEVTX * sizeof(GLuint), indexes, GL_STATIC_DRAW);
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
    #ifdef DYNASCROLL
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
    
    CreateThread(0, 0x4000, PlayerThread,    ply, 0, 0);
    CreateThread(0, 0x4000, PlayerThread, player, 0, 0);
    
    QWORD currnotealloc = 0;
    
    int(WINAPI*NtQuerySystemTime)(QWORD* timeptr) = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtQuerySystemTime");
    
    QWORD prevtime;
    NtQuerySystemTime(&prevtime);
    QWORD currtime = prevtime;
    
    DWORD timeout = 0;
    
    #if defined(ROUNDEDGE) || defined(TRIPPY)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    #endif
    
    #ifdef TRANSFORM
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    #endif
    
    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_CCW);
    
    #ifdef KEYBOARD
    ULONGLONG lasttimer = 0;
    ULONGLONG currtimer = 0;
    
    NoteNode KeyNotes[256];
    ZeroMemory(KeyNotes, sizeof(KeyNotes));
    #endif
    
    while(!(WaitForSingleObject(vsyncevent, timeout) >> 9))
    {
        ResetEvent(vsyncevent);
        
        glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferData(GL_ARRAY_BUFFER, datasize, 0, GL_STREAM_DRAW);
        
        glEnableVertexAttribArray(attrVertex);
        glEnableVertexAttribArray(attrColor);
        
        glVertexAttribPointer(attrVertex, 2, GL_FLOAT, GL_FALSE, 16, 0);
        glVertexAttribPointer(attrColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)8);
        
        glUseProgram(sh);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
        
        ULONGLONG notesdrawn = 0;
        
        #ifdef DYNASCROLL
        ULONGLONG notesync = mmnotesync;
        ULONGLONG currtick = syncvalue;
        #else
        ULONGLONG notesync = TICKVAR;
        ULONGLONG currtick = TICKVAL;
        DWORD tickheight = (2500000
        #ifdef TRANSFORM
        * 4
        #endif
        ) / player->tempomulti;
        float tickscale = 1.0F / (float)tickheight;
        #endif
        
        #ifdef KEYBOARD
        currtimer = player->RealTime;
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
        
        NoteNode* prevnote = 0;
        NoteNode* currnote = VisibleNoteList;
        
        //loop over freeable notes first
        while(currnote && currnote->start <= currtick) // already triggered
        {
            #ifdef KEYBOARD
            NoteNode* lmn = &KeyNotes[(BYTE)currnote->uid];
            
            if(lmn->uid != currnote->uid)
            {
                lmn->uid = currnote->uid;
                lmn->start = 14000000;
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
                
                notesdrawn++;
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
                    if(VisibleNoteList == VisibleNoteListHead)
                    {
                        VisibleNoteList = 0;
                        VisibleNoteListHead = 0;
                        
                        NoteFree(currnote);
                        currnote = 0;
                        break;
                    }
                    
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
            
            notesdrawn++;
        }
        
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
        
        ULONGLONG delta = 40000000;
        
        if((currtimer - lasttimer) < 2500000) // 250ms
            delta = (currtimer - lasttimer) * 8;
        
        #ifdef PIANOKEYS
        BOOL blackflag = FALSE;
        int keyflag = 0;
        int posflag = 0;
        #else
        #define posflag i
        #endif
        
        
        #ifdef WIDEMIDI
        for(int i = 0; i < 256;)
        #else
        for(int i = 0; i < 128;)
        #endif
        {
            NoteNode* lmn = &KeyNotes[i];
            DWORD dwColor =
                #ifdef PIANOKEYS
                    blackflag
                #else
                    1
                #endif
                ? 0xFF111111 : 0xFF808080;
            
            if(lmn->start)
            {
                dwColor = LerpColor(dwColor, ((colortable[lmn->uid >> 8] & 0xFEFEFEFE) >> 1) | (0xFF << 24), (float)lmn->start * 0.0000001F);
                
                if(lmn->start > delta)
                    lmn->start -= delta;
                else
                    lmn->start = 0;
            }
            
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
                , -1.0F, posflag, posflag + 
            #ifdef PIANOKEYS
                2
            #else
                1
            #endif
                , dwColor);
                
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
        
        lasttimer = currtimer;
        
        #endif
        
        if(vtxidx)
            FlushToilet();
        //printf("Drawn: %10llu | Desync: %10lli\n", notesdrawn, player->RealTime - ply->RealTime);
        notesdrawn = 0;
        //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        //glDrawArrays(GL_TRIANGLES, 0, 3);
        //glDrawElements(GL_TRIANGLES, 6 * vtxidx, GL_UNSIGNED_INT, indexes);
        
        if(notealloccount != currnotealloc)
        {
            currnotealloc = notealloccount;
            printf("Note allocation changed: %14llu\n", currnotealloc);
        }
        
        glDisableVertexAttribArray(attrVertex);
        glDisableVertexAttribArray(attrColor);
        
        glUseProgram(0);
        //glFlush();
        
        InvalidateRect(glwnd, 0, 0);
        uglSwapBuffers(dc);
        
        glViewport(erect.left, erect.top, erect.right - erect.left, erect.bottom - erect.top);
        
        NtQuerySystemTime(&currtime);
        if((currtime - prevtime) >> 18)
            timeout = 0;
        else
            timeout = 30;
        prevtime = currtime;
        
        if(player->tracks->ptrs)
            continue;
        
        puts("MIDI end");
        
        glFinish();
        free(quads);
        
        while(notelist)
        {
            NoteNode* mn = notelist->next;
            free(notelist);
            notelist = mn;
        }
        
        break;
    }
    
    ded:
    
    wglMakeCurrent(0, 0);
    
    puts("Renderer died");
    
    return 0;
}
