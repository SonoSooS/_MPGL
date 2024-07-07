
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"

#include "GL/ctor.h"
#include "GL/core.h"
#include "GL/utility.h"

#include "render.h"
#include "render_grh.h"
#include "render_grf.h"

#include "player/mmplayer.h"

#include "ctrufont.h"


struct textquadpart
{
    float x, y;
    GLuint color;
    WORD u, v;
};

struct textquad
{
    struct textquadpart quads[4];
};


extern MMPlayer* player;

extern ULONGLONG currnps;
extern ULONGLONG drawnotesraw;
extern ULONGLONG drawnotes;

extern QWORD midisize;

#ifdef TEXTNPS
extern struct histogram* hist;
extern ULONGLONG notecounter;
#endif

#ifdef WMA_SIZE
extern ULONGLONG* fps_wma;
extern DWORD fps_wmai;
#endif

static DWORD notetimer = 0;



GLuint shGrfFontShader;
GLint attrGrfFontColor;
GLint attrGrfFontVertex;
GLint attrGrfFontUV;
GLint uniGrfFontTime;
GLint uniGrfFontNPS;
GLint uniGrfFontTexi;
GLint uniGrfFontBgColor;

GLuint texGrfFont;


void grfFontSetBg(DWORD color)
{
    float bgc[4];
    bgc[0] = (BYTE)(color >> 0) / 255.0F;
    bgc[1] = (BYTE)(color >> 8) / 255.0F;
    bgc[2] = (BYTE)(color >> 16) / 255.0F;
    bgc[3] = (BYTE)(color >> 24) / 255.0F;
    
    glUniform4fv(uniGrfFontBgColor, 1, (GLfloat*)bgc);
}

void grfDrawFontString(int32_t x, int32_t y, int32_t scale, DWORD color, const char* text)
{
    DWORD startx = x;
    
    //while(y < 72)
    for(;;)
    {
        char c = *(text++);
        if(!c)
            break;
        
        if(c == '\r')
        {
            x = startx;
            continue;
        }
        else if(c == '\n')
        {
            x = startx;
            y += scale;
            
            continue;
        }
        
        if(c != ' ')
        {
            float offsx = x;
            float offsr = x + scale;
            float offsy = y;
            float offst = y + scale;
            
            DWORD ux =  (c       & 0xF) << 3;
            DWORD uy = ((c >> 4) & 0xF) << 3;
            
            struct textquad* ck = ((struct textquad*)quads) + (vtxidx++);
            ck->quads[0] = (struct textquadpart){offsx, offst, color, ux,     uy    };
            ck->quads[1] = (struct textquadpart){offsx, offsy, color, ux,     uy + 8};
            ck->quads[2] = (struct textquadpart){offsr, offsy, color, ux + 8, uy + 8};
            ck->quads[3] = (struct textquadpart){offsr, offst, color, ux + 8, uy    };
        }
        
        x += scale;
        
        if(x >= 128)
        {
            x = startx;
            y += scale;
        }
    }
}

void grfDrawFontOverlay(void)
{
    if(vtxidx)
    {
        printf("Partial rendering detected! Buffer is at %i\n", vtxidx);
        vtxidx = 0;
    }
    
    #ifdef TRANSFORM
    glClear(GL_DEPTH_BUFFER_BIT);
    #endif
    
    #ifdef TEXTTRANS
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    #endif
    
    //glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texGrfFont);
    glUseProgram(shGrfFontShader);
    
    glEnableVertexAttribArray(attrGrfFontVertex);
    glEnableVertexAttribArray(attrGrfFontColor);
    glEnableVertexAttribArray(attrGrfFontUV);
    
    glVertexAttribPointer(attrGrfFontVertex, 2, GL_FLOAT, GL_FALSE, 16, 0);
    glVertexAttribPointer(attrGrfFontColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)8);
    glVertexAttribPointer(attrGrfFontUV, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16, (void*)12);
    
    #ifdef SHNPS
    glUniform1f(uniGrfFontTime, (float)((double)(player->RealTime) / 1e7));
    glUniform1f(uniGrfFontNPS, (float)currnps);
    #endif
    
    grfFontSetBg(0xBF << 24);
    
    int textlen;
    char buf[256];
    
    #ifndef TEXTNEAT
    const int32_t ybase = 0;
    #else
    const int32_t ybase = 70;
    #endif
    
    #ifndef TEXTCUSTOM1
        #define TEXTL -textlen
        #define TEXTR -textlen
        #define TEXTU 10
        #define TEXTD -7
        #define TEXTROFFS(offs) -textlen
    #else
        #define TEXTL -128
        #define TEXTR 128-textlen-textlen
        #define TEXTU 70
        #define TEXTD 70
        #define TEXTROFFS(offs) (TEXTR - offs)
    #endif
    
    #ifdef TEXTNPS
    textlen = sprintf(buf, "%llu N/s ", currnps);
    grfDrawFontString(TEXTROFFS(2), ybase - 2, 2, -1, buf);
    #endif
    
    #if !defined(TEXTCUSTOM1) || 1
    textlen = sprintf(buf, "%llu NoS ", drawnotes);
    grfDrawFontString(TEXTROFFS(2), ybase - /*0*/ 4, 2, -1, buf);
    #endif
    
    
    textlen = sprintf(buf, " %llu notes", notecounter);
    grfDrawFontString(TEXTR, ybase - 0, 2, -1, buf);
    
    #ifdef WMA_SIZE
    ULONGLONG wma = 0;
    for(DWORD i = 0; i != WMA_SIZE; i++)
    {
        wma += fps_wma[i];
    }
    
    double wmad = wma / (double)WMA_SIZE;
    
    {
        #ifndef TEXTCUSTOM1
            int32_t ty = ybase - 6;
        #else
            int32_t ty = ybase - 0;
        #endif
        
        textlen = sprintf(buf, "%.2f FPS", 1e7 / wmad);
        grfDrawFontString(TEXTL, ty, 2, -1, buf);
    }
    #endif
    
    #ifdef TEXTALLOC
    if(notealloccount != currnotealloc)
    {
        currnotealloc = notealloccount;
        
        notetimer = (DWORD)2e7;
    }
    
    if(notetimer)
    {
        DWORD dwColor = -1;
        
        #ifndef TEXTNEAT
        if(notetimer < (1 << 24))
        {
            dwColor = (notetimer >> 16);
            dwColor |= dwColor << 8;
            dwColor |= dwColor << 16;
        }
        #endif
        
        if(notetimer > (1 << 16))
            notetimer -= (1 << 16);
        else
            notetimer = 0;
        
        #undef TEXTL
        #undef TEXTR
        #undef TEXTU
        #undef TEXTD
        
        #ifndef TEXTNEAT
            #define TEXTL -textlen
            #define TEXTR -textlen
            #define TEXTU 10
            #define TEXTD -7
        #else
            #define TEXTL -128
            #define TEXTR 128-textlen-textlen
            #define TEXTU 70
            #define TEXTD 70
        #endif
        
        textlen = 18;
        grfDrawFontString(TEXTL, TEXTU - 1, 2, dwColor, "__________________");
        grfDrawFontString(TEXTL, TEXTU - 0, 2, dwColor >> 1, "Memory allocation!");
        
        textlen = sprintf(buf, "%llu slots", currnotealloc);
        grfDrawFontString(TEXTL, TEXTU - 4, 2, dwColor, buf);
        
        textlen = sprintf(buf, "%.3fMB slots",
                          (float)(currnotealloc * sizeof(NoteNode)) / (1024.0F * 1024.0F));
        grfDrawFontString(TEXTR, TEXTD - 3, 2, dwColor, buf);
        
        textlen = sprintf(buf, "%.3fMB total",
                          (float)(midisize + (currnotealloc * sizeof(NoteNode))) / (1024.0F * 1024.0F));
        grfDrawFontString(TEXTR, TEXTD - 0, 2, dwColor >> 1, buf);
    }
    #endif
    
    #ifdef DEBUGTEXT
    textlen = sprintf(buf, "BPM:   %06X | %i (%6.4f)", player->tempo, player->tempo, 60000000 / (float)player->tempo);
    grfDrawFontString(-128, -60, 2, -1, buf);
    
    textlen = sprintf(buf, "Time:  %llu / %u == %llu (%llu + %llu)",
        player->RealTimeUndiv, player->timediv,
        player->RealTimeUndiv / player->timediv,
        player->RealTime, (player->RealTimeUndiv / player->timediv) - player->RealTime);
    grfDrawFontString(-128, -58, 2, -1, buf);
    
    textlen = sprintf(buf, "Ticks: %llu", player->TickCounter);
    grfDrawFontString(-128, -56, 2, -1, buf);
    
    textlen = sprintf(buf, "TPS:   %f", 1e6 * player->timediv / (double)player->tempo);
    grfDrawFontString(-128, -54, 2, -1, buf);
    
    
    #ifdef EXTREMEDEBUG
    
    int32_t debugbase = 60;
    int32_t notex = -126;
    
    uint32_t nactivid = 0;
    NoteNode* note = ActiveNoteList[nactivid];
    uint32_t nactivmax = trackcount - 1;
    
    do
    {
        while(!note && nactivid < nactivmax)
            note = ActiveNoteList[++nactivid];
        
        if(note)
        {
            sprintf(buf, "%8X %20llu %lli", note->uid, note->start, (!~note->end) ? -1LL : (note->end - note->start));
            grfDrawFontString(notex, debugbase, 2, -1, buf);
            
            
            note = note->overlapped_voce;
            
        }
        else if(nactivid >= nactivmax)
        {
            grfDrawFontString(notex, debugbase, 2, -1, "[end]");
            break;
        }
        
        debugbase -= 2;
    }
    while(debugbase > -50);
        
    if(note || nactivid < 0xFFF)
        grfDrawFontString(notex, debugbase - 2, 2, -1, "[...]");
    
    
    notex = 0;
    debugbase = 60;
    
    note = VisibleNoteList;
    
    if(!note)
        grfDrawFontString(notex, debugbase, 2, -1, "* No active notes for this channel*");
    else
    {
        do
        {
            if(note)
            {
                sprintf(buf, "%8X %20llu %lli", note->uid, note->start, !~note->end ? -1LL : (note->end - note->start));
                grfDrawFontString(notex, debugbase, 2, -1, buf);
                
                
                note = note->next;
                
            }
            else
            {
                grfDrawFontString(notex, debugbase, 2, -1, "[end]");
                break;
            }
            
            debugbase -= 2;
        }
        while(debugbase > -50);
        
        if(note)
            grfDrawFontString(notex, debugbase - 2, 2, -1, "[...]");
    }
    
    /*
    sprintf(buf, "slp: %10i", player->_debug_sleeptime);
    grfDrawFontString(-126, 70, 2, -1, buf);
    sprintf(buf, "lag: %10i", player->_debug_deltasleep);
    grfDrawFontString(-126, 68, 2, -1, buf);
    */
    
    #endif
    
    #endif
    
    if(vtxidx)
    {
        //glBufferData(GL_ARRAY_BUFFER, vtxidx * sizeof(struct textquad),     0, GL_STREAM_DRAW);
        //glBufferData(GL_ARRAY_BUFFER, vtxidx * sizeof(struct textquad), quads, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vtxidx * sizeof(struct textquad), quads);
        glDrawElements(GL_TRIANGLES, vtxidx * NOTEVTX, GL_UNSIGNED_INT, 0);
        
        vtxidx = 0;
    }
    
    glDisableVertexAttribArray(attrGrfFontUV);
    glDisableVertexAttribArray(attrGrfFontColor);
    glDisableVertexAttribArray(attrGrfFontVertex);
    
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    //glActiveTexture(0);
    //glDisable(GL_TEXTURE_2D);
    
    #ifdef TEXTTRANS
    glDisable(GL_BLEND);
    #endif
}

void grfInstallShader(void)
{
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint psh = glCreateShader(GL_FRAGMENT_SHADER);
    
    
    const char* shadera =
        "#version 330 core\n"
    #ifdef SHNPS
        "uniform float intime;\n"
        "uniform float innps;\n"
    #endif
        "in vec4 incolor;\n"
        "in vec2 inpos;\n"
        "in vec2 inuv;\n"
        "out vec4 pcolor;\n"
        "out vec2 puv;\n"
        "void main()\n"
        "{\n"
        "   vec2 rawpos = vec2(inpos.x, inpos.y * 16.0F / 9.0F);\n"
        "   pcolor = incolor;\n"
        "   puv = inuv * (1.0F / 128.0F);\n"
        "   vec2 pos = vec2((rawpos.x * (1.0F / 128.0F)), rawpos.y * (1.0F / 128.0F));\n"
        #if defined(SHNPS) && defined(SHWOBBLE)
        "   float shfactor = min((1.0F / 64.0F), pow(max(0.0F, innps - 4096.0F), 0.5F) * (1.0F / 32768.0F));\n"
        "   vec2 vtxpos = vec2(pos.x + (sin((rawpos.y * 3.0F + (rawpos.x * 17.0F) + (intime * 0.125F * innps))) * shfactor),\n"
        "                      pos.y + (cos((rawpos.x * 7.0F                      + (intime * innps))) * shfactor));\n"
        #else
        "   vec2 vtxpos = pos;\n"
        #endif
        #if defined(KEYBOARD) && !defined(ROTAT)
        "   vtxpos.y = (vtxpos.y * 0.8F) + 0.2F;\n"
        #endif
        "   gl_Position = vec4(vtxpos.xy, 0.0F, 1.0F);\n"
        "}\n"
        ;

    const char* shaderb =
        "#version 330 core\n"
        "uniform sampler2D fonTex;\n"
        "uniform vec4 bgcolor;\n"
        "in vec4 pcolor;\n"
        "in vec2 puv;\n"
        "out vec4 outcolor;\n"
        "void main()\n"
        "{\n"
        "   outcolor = mix(bgcolor, pcolor, texture2D(fonTex, puv));\n"
        "}\n"
        ;
    
    GLint stat = 0;
    
    glShaderSource(vsh, 1, &shadera, 0);
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        grhPrintShaderInfoLog(vsh);
    
    glShaderSource(psh, 1, &shaderb, 0);
    glCompileShader(psh);
    glGetShaderiv(psh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        grhPrintShaderInfoLog(psh);
    
    shGrfFontShader = glCreateProgram();
    
    glAttachShader(shGrfFontShader, vsh);
    glAttachShader(shGrfFontShader, psh);
    
    glLinkProgram(shGrfFontShader);
    
    glGetProgramiv(shGrfFontShader, GL_LINK_STATUS, &stat);
    //if(stat != 1)
        grhPrintProgramInfoLog(shGrfFontShader);
    
    attrGrfFontVertex = glGetAttribLocation(shGrfFontShader, "inpos");
    attrGrfFontColor = glGetAttribLocation(shGrfFontShader, "incolor");
    attrGrfFontUV = glGetAttribLocation(shGrfFontShader, "inuv");
    
    if(attrGrfFontVertex < 0)
        puts("inpos not found");
    if(attrGrfFontColor < 0)
        puts("incolor not found");
    if(attrGrfFontUV < 0)
        puts("inuv not found");
    
    if(attrGrfFontVertex < 0 || attrGrfFontColor < 0 || attrGrfFontUV < 0)
        __builtin_trap();
    
    
    uniGrfFontTime = glGetUniformLocation(shGrfFontShader, "intime");
    uniGrfFontNPS = glGetUniformLocation(shGrfFontShader, "innps");
    uniGrfFontTexi = glGetUniformLocation(shGrfFontShader, "fonTex");
    uniGrfFontBgColor = glGetUniformLocation(shGrfFontShader, "bgcolor");
    
    //glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texGrfFont);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texGrfFont);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    void* fonttex = ctru_unpack();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_BGRA, GL_UNSIGNED_BYTE, fonttex);
    free(fonttex);
    
    glUseProgram(shGrfFontShader);
    glUniform1i(uniGrfFontTexi, 0);
    #ifdef SHNPS
    if(uniGrfFontTime >= 0)
        glUniform1f(uniGrfFontTime, 0);
    if(uniGrfFontNPS >= 0)
        glUniform1f(uniGrfFontNPS, 0);
    #endif
    glUseProgram(0);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    //glActiveTexture(0);
    //glDisable(GL_TEXTURE_2D);
    
    glDetachShader(shGrfFontShader, psh);
    glDetachShader(shGrfFontShader, vsh);
    
    glDeleteShader(vsh);
    glDeleteShader(psh);
    
    #ifdef TEXTNPS
    hist = malloc((DWORD)1e7 * 0x10);
    ZeroMemory(hist, (DWORD)1e7 * 0x10);
    midisize += (DWORD)1e7 * 0x10;
    notecounter = 0;
    #endif
}
