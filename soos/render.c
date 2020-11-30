#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "GL/ctor.h"
#include "GL/core.h"
#include "GL/utility.h"

#include "player/mmplayer.h"


#define TRACKID
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
#define GLOW
//#define SHTIME
//#define WOBBLE
//#define WOBBLE_INTERP
//#define GLOWEDGE
//#define GLTEXT
//#define TEXTNPS
//#define SHNPS
//#define HDR
//#define NOKEYBOARD
#define TIMI_CAPTURE
#define TIMI_IMPRECISE
//#define FASTHDR


#ifdef PIANOKEYS
const DWORD keymul = 8;
#else
const DWORD keymul = 1;
#endif

#ifdef TIMI_CAPTURE
const ULONGLONG FPS_NOMIN = 1;
const ULONGLONG FPS_DENIM = 60;
ULONGLONG FPS_frame = 0;
volatile BOOL FPS_capture = FALSE;
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

#ifndef HDR
typedef DWORD KCOLOR;
#else
typedef struct { float r, g, b, a; } KCOLOR;

#if defined(GLTEXT) || defined(TEXTNPS)
#error HDR is incompatible with some modules
#endif
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

extern BOOL canrender;

extern HMODULE KSModule;

GLuint sh;
GLint attrVertex, attrColor;
#ifdef SHTIME
GLint uniTime;
#endif
#ifdef HDR
GLint uniLightAlpha;
GLint uniLightColor;
#ifdef TRIPPY
GLint attrNotemix;
#endif
#endif

#ifndef HDR
struct quadpart
{
    float x, y;
    GLuint color;
    WORD u, v;
};
#else
struct quadpart
{
    float x, y;
    KCOLOR color;
};
#endif

struct quad
{
    struct quadpart quads[ QUADCOUNT ];
};

static void PrintShaderInfo(GLuint shader)
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

static void CompileStandardShader()
{
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint psh = glCreateShader(GL_FRAGMENT_SHADER);
    
    
    const char* shadera =
        "#version 330 core\n"
    #ifdef SHTIME
        "uniform float intime;\n"
    #endif
        "in vec4 incolor;\n"
        "in vec2 inpos;\n"
    #ifndef PFAKEY
        "flat "
    #endif
        "out vec4 pcolor;\n"
    #ifdef TRANSFORM
        "out float zbuf;"
    #endif
    #if defined(ROUNDEDGE) || defined(GLOWEDGE)
        "out vec2 ppos;\n"
        "flat out vec2 fpos;\n"
    #endif
    #ifdef HDR
        "out vec2 npos;\n"
        #ifdef TRIPPY
            "out float illum;\n"
        #endif
    #endif
    #ifdef PIANOKEYS
        "const float divider = 8.0F;\n"
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
    #ifdef GLOW
        "   float ill_a = "
    #if defined(TRIPPY) && defined(TRANSFORM)
        "   clamp((-rawpos.y - 0.95F) * 8.0F, 0.0F, 6.0F);\n"
    #elif defined(TRANSFORM)
        "   clamp((-rawpos.y - 0.95F) * 32.0F, 1.0F, 6.0F);\n"
    #elif defined(TRIPPY) && !defined(HDR)
        "   clamp((-rawpos.y - 0.5F) * 4.0F, 0.0F, 5.6F);\n"
    #else
        "   clamp((-rawpos.y - 0.865F) * 16.0F, 1.0F, 5.6F);\n"
    #endif
        #if defined(HDR) && defined(TRIPPY)
        "   pcolor = incolor;\n"
        #else
        "   pcolor = vec4(incolor.xyz * ill_a, incolor.w);\n"
        #endif
    #else
        "   pcolor = incolor;\n"
    #endif
    #ifdef PIANOKEYS
        "   vec2 pos = vec2((rawpos.x * (4.0F / (150.0F * divider))) - 1.0F, rawpos.y);\n"
    #else
        "   vec2 pos = vec2((rawpos.x * (2.0F / 128.0F)) - 1.0F, rawpos.y);\n"
    #endif
    #ifdef TRANSFORM
        "   float z = min((1.0F - pos.y) * 0.5F, 1.0F);\n"
        //"   vec2 vtxpos = vec2(pos.x * ((3.0F - pos.y) * 0.25F), (-z*z*2.0F) + 1.0F);\n"
        "   vec2 vtxpos = vec2(pos.x * ((3.0F - max(pos.y, -1.0F)) * 0.25F), (-pos.y*pos.y*2.0F) + 1.0F);\n"
        "   zbuf = z;\n"
    #else
        "   vec2 vtxpos = pos;\n"
    #endif
    #if defined(ROUNDEDGE) || defined(GLOWEDGE)
        "   ppos = vtxpos;\n"
        "   fpos = vtxpos;\n"
    #endif
    #ifdef KEYBOARD
    #ifdef ROTAT
        "   vtxpos.y = max((vtxpos.y * 0.8F) + 0.2F, -0.7F);\n"
    #else
        "   vtxpos.y = (vtxpos.y * 0.8F) + 0.2F;\n"
    #endif
        //"   vtxpos.y = pow(vtxpos.y + 1.0F, 1.4F) - 1.0F;"
    #endif
    #ifdef ROTAT
        "   const float PI = 3.1415926535897932384626433832795;\n"
        "   vtxpos = vec2(sin((vtxpos.x + 1.0F) * PI), cos((vtxpos.x + 1.0F) * PI) * (16.0F / 9.0F)) * (((vtxpos.y + 1.0F) * 0.75F) - 0.05F);\n"
    #endif
    #ifdef WOBBLE
        "   vec2 interm = vec2(vtxpos.x + cos(intime*4.0F + vtxpos.x * 8.0F)*0.025F + sin(vtxpos.y + cos(intime)*0.25F * 4.0F)*0.125F, vtxpos.y + cos((sin(intime * 0.8F) + vtxpos.y) * 2.0F)*0.25F + sin(vtxpos.x * 4.0F)*0.125F + sin(vtxpos.y*4.0F + intime*2.769F)*0.025F);\n"
    #ifdef WOBBLE_INTERP
        "   float interp_amt = pos.y + 0.02F;\n"
        "   vtxpos.xy = mix(interm, vtxpos, min(-interp_amt * interp_amt * interp_amt, 1.0F));\n"
    #else
        "   vtxpos.xy = interm;\n"
    #endif
    #endif
    #ifdef HDR
        "   npos = vtxpos.xy;\n"
        #ifdef TRIPPY
            #ifdef GLOW
            "    illum = clamp(((-rawpos.y - 0.95F) * 32.0F), 0.0F, 5.6F);\n"
            #endif
        #endif
    #endif
    #ifdef TRANSFORM
        "   gl_Position = vec4(vtxpos.xy, -z, 1.0F);\n"
    #else
        "   gl_Position = vec4(vtxpos.xy, 0.0F, 1.0F);\n"
    #endif
        "}\n"
        ;

    #ifndef HDR
    const char* shaderb =
    #else
    const char* shaderb[] =
    {
    #endif
        "#version 330 core\n"
    #ifndef PFAKEY
        "flat "
    #endif
        "in vec4 pcolor;\n"
    #ifdef TRANSFORM
        "in float zbuf;\n"
    #endif
    #if defined(ROUNDEDGE) || defined(GLOWEDGE)
        "in vec2 ppos;\n"
        "flat in vec2 fpos;\n"
        "vec3 saturate(vec3 c, float amt)\n"
        "{\n"
        "    vec3 gray = vec3(dot(vec3(0.2126F,0.7152F,0.0722F), c));\n"
        "    return vec3(mix(gray, c, amt));\n"
        "}\n"
    #endif
    #ifdef HDR
        "in vec2 npos;\n"
        #ifdef TRIPPY
        "in float illum;\n"
        "uniform float notemix;\n"
        #endif
        #ifdef WIDEMIDI
        "uniform float lighta[256];\n"
        "uniform vec4  lightc[256];\n"
        #else
        "uniform float lighta[128];\n"
        "uniform vec4  lightc[128];\n"
        #endif
        "uniform sampler2D blurtex;\n"
        "vec2 texres = vec2(1.0F / 1280.0F, 1.0F / 720.0F);\n"
        "vec4 blur(sampler2D tex, vec2 uv)\n"
        "{\n"
        "    vec4 color = vec4(0.0F);\n"
        "    vec2 blurk[3];\n"
        "    blurk[0] = 1.4117647058823530F * texres;\n"
        "    blurk[1] = 3.2941176470588234F * texres;\n"
        "    blurk[2] = 5.1764705882352940F * texres;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y - blurk[2].y)) * 0.015302F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y - blurk[1].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y - blurk[1].y)) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y - blurk[1].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[1].x, uv.y - blurk[0].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y - blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y - blurk[0].y)) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y - blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[1].x, uv.y - blurk[0].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[2].x, uv.y             )) * 0.015302F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[1].x, uv.y             )) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y             )) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y             )) * 0.046056F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y             )) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[1].x, uv.y             )) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[2].x, uv.y             )) * 0.015302F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[1].x, uv.y + blurk[0].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y + blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y + blurk[0].y)) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y + blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[1].x, uv.y + blurk[0].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y + blurk[1].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y + blurk[1].y)) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y + blurk[1].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y + blurk[2].y)) * 0.015302F;\n"
        "    \n"
        "    return color;\n"
        "}\n"
    #endif
        "out vec4 outcolor;\n"
        "void main()\n"
        "{\n"
    #if defined(ROUNDEDGE)
        "   vec2 rpos = ppos - fpos;\n"
        "   float af = (64.0F * abs(rpos.x*rpos.y)) + ((rpos.x*rpos.x)+(rpos.y*rpos.y));\n"
        "   float ua = af * 1024.0F;\n"
        "   float a = clamp(ua, 0.0F, 1.0F);\n"
        "   outcolor = vec4(pcolor.xyz, pcolor.w * a)"
        #ifdef TRANSFORM
            " * (0.2F + zbuf)"
        #endif
        ";\n"
    #elif defined(GLOWEDGE)
        "   vec2 rpos = abs(ppos - fpos);\n"
        "   float af = rpos.x * rpos.y;\n"
        "   float ua = af * 1024.0F;\n"
        "   float a = clamp(1.0F - (ua * ua), 0.0F, 1.0F);\n"
        "   outcolor = vec4(saturate(pcolor.xyz, ((a * a)) + 1.0F), pcolor.w)"
        #ifdef TRANSFORM
            " * (0.2F + zbuf)"
        #endif
        ";\n"
    #elif defined(HDR)
        "   outcolor = pcolor;//blur(blurtex, gl_FragCoord.xy * texres) + pcolor;\n"
        "   vec4 lightcolor = vec4(0.0F, 0.0F, 0.0F, 1.0F);\n"
        #ifdef FASTHDR
        "   if(notemix == 0.0F){\n"
        #endif
        #ifdef WIDEMIDI
        "   for(int i = 0; i < 256; i++)\n"
        #else
        "   for(int i = 0; i < 128; i++)\n"
        #endif
        "   {\n"
        #ifdef ROTAT
        "       const float PI = 3.1415926535897932384626433832795;\n"
        "       float xpos = (((i - 128) / 64.0F) + 1.0F + (1.0F / 151.0F)) * PI;\n"
        "       vec2 posdiff = npos - vec2(sin(xpos) * -0.25F, cos(xpos) * -0.25F * (16.0F / 9.0F));\n"
        #else
        "       vec2 posdiff = npos - vec2(((i - 128) / 64.0F) + 1.0F + (1.0F / 151.0F), -0.6F);\n"
        #endif
        #ifndef TRIPPY
        //"       vec2 asposdif = posdiff;\n"
        "       vec2 asposdif = vec2(posdiff.x, posdiff.y * (9.0F / 16.0F));\n"
        "       float posdist = dot(asposdif, asposdif);\n"
        "       lightcolor += vec4((lightc[i].xyz * lighta[i]) * min(pow(1.0F / posdist, 1.0F) * (1.0F / 8192.0F), 4.0F), 0.0F);\n"
        #else
        "       vec2 asposdif = vec2(posdiff.x, posdiff.y * (1.0F / 4.0F));\n"
        "       float posdist = dot(asposdif, asposdif);\n"
        "       lightcolor += vec4((lightc[i].xyz * pow(lighta[i], 1.0F)) * min(pow((0.5F / posdist), 1.0F) * (1.0F / 4096.0F), 4.0F), 0.0F);\n"
        #endif
        "   }\n"
        #ifdef FASTHDR
        "   }\n"
        #endif
        #if defined(TRIPPY)
        "   float notea = 1.0F - notemix;\n"
        //"   float sosi = dot(lightcolor.xyz, pcolor.www) * 8.0F;\n"
        "   float sosi = max(1.0F, illum);\n"
        "   outcolor = vec4("
                //"(lightcolor.xyz * notemix) +"
                "(pcolor.xyz * vec3(mix(sosi, 1.0F, notea)))"
                " * (((("
                        "pow(lightcolor.xyz, vec3(0.8F))"
                        " * vec3(4.0F))"
                    " + vec3((1.0F / 64.0F))"
                    ") * vec3(notea)) + vec3(notemix))"
            ", outcolor.w);\n"
        
        /*
        "   outcolor = vec4("
        //"(lightcolor.xyz * notemix) +"
        "(pcolor.xyz * vec3((notemix * sosi) + 1.0F)) * (((pow(lightcolor.xyz, vec3((notea * -0.25F) + 1.0F)) * ((notea * 0.4F) + 1.0F)) * vec3(notea)) + ((1.0F / 32.0F))), outcolor.w);\n"
        */
        #elif defined(TRANSFORM)
        "   outcolor = vec4(outcolor.xyz * (0.2F + (zbuf*zbuf)), outcolor.w);\n"
        #else
        "   outcolor += lightcolor;\n"
        #endif
        #ifdef NOKEYBOARD
            #if defined(ROTAT)
            "   outcolor = vec4(mix(outcolor.xyz, lightcolor.xyz, clamp((-npos.y - 0.58F) * 32.0F, 0.0F, 1.0F)), outcolor.w);\n"
            #elif !(defined(HDR) && defined(TRIPPY))
            "   vec2 corrpos = vec2(npos.x, npos.y * (9.0F / 16.0F));\n"
            "   outcolor = vec4(mix(outcolor.xyz, lightcolor.xyz, clamp((0.09F - dot(corrpos, corrpos)) * 32.0F, 0.0F, 1.0F)), outcolor.w);\n"
            #endif
        #endif
        ,
        "",
    #else
        "   outcolor = vec4(pcolor.xyz"
        #ifdef TRANSFORM
            " * (0.2F + (zbuf*zbuf))"
        #endif
        ", pcolor.w);\n"
    #endif
        "}\n"
    #ifdef HDR
    }
    #endif
    ;
    GLint stat = 0;
    
    glShaderSource(vsh, 1, &shadera, 0);
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        PrintShaderInfo(vsh);
    
    #ifdef HDR
    #ifndef TIMI_CAPTURE
    if(!uglSupportsExt("WGL_EXT_framebuffer_sRGB"))
    #endif
    {
        shaderb[1] =
        //"   outcolor = vec4(pow(outcolor.xyz / (outcolor.xyz + vec3(1.0F)), vec3(1.1F)), outcolor.w);\n"
        "   outcolor = vec4(pow(outcolor.xyz, vec3(0.5F)), outcolor.w);\n"
        ;
    }
    glShaderSource(psh, 3, shaderb, 0);
    #else
    glShaderSource(psh, 1, &shaderb, 0);
    #endif
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
    
    while(attrVertex < 0 || attrColor < 0)
        ;
    
    #ifdef SHTIME
    uniTime = glGetUniformLocation(sh, "intime");
    #endif
    
    #ifdef HDR
    uniLightAlpha = glGetUniformLocation(sh, "lighta");
    uniLightColor = glGetUniformLocation(sh, "lightc");
        #ifdef TRIPPY
        attrNotemix = glGetUniformLocation(sh, "notemix");
        #endif
    #endif
    
    glDetachShader(sh, psh);
    glDetachShader(sh, vsh);
    
    glDeleteShader(vsh);
    glDeleteShader(psh);
}

extern QWORD midisize;

#ifdef GLTEXT
#include "ctrufont.h"

GLuint texFont;

#ifdef TEXTNPS
struct histogram* hist;
#endif
#ifdef SHNPS

GLint uniFontTime;
GLint uniFontNPS;
#endif
GLint uniFontTex;

GLuint fontsh;
GLint attrFontColor;
GLint attrFontVertex;
GLint attrFontUV;


ULONGLONG drawnotesraw = 0;
ULONGLONG drawnotes = 0;

void CompileFontShader()
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
        #ifdef SHNPS
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
        "uniform sampler2D fonTex;"
        "in vec4 pcolor;\n"
        "in vec2 puv;\n"
        "out vec4 outcolor;\n"
        "void main()\n"
        "{\n"
        "   outcolor = texture2D(fonTex, puv) * pcolor;\n"
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
    
    fontsh = glCreateProgram();
    
    glAttachShader(fontsh, vsh);
    glAttachShader(fontsh, psh);
    
    glLinkProgram(fontsh);
    
    glGetProgramiv(fontsh, GL_LINK_STATUS, &stat);
    //if(stat != 1)
    {
        glGetProgramiv(fontsh, GL_INFO_LOG_LENGTH, &stat);
        if(stat > 0)
        {
            char ilog[256];
            GLsizei outloglen = 0;
            glGetProgramInfoLog(fontsh, 256, &outloglen, ilog);
            printf("%*.*s\n", outloglen, outloglen, ilog);
        }
        else
        {
            //puts("Unknown shader program error");
        }
        
    }
    
    attrFontVertex = glGetAttribLocation(fontsh, "inpos");
    attrFontColor = glGetAttribLocation(fontsh, "incolor");
    attrFontUV = glGetAttribLocation(fontsh, "inuv");
    
    if(attrFontVertex < 0)
        puts("inpos not found");
    if(attrFontColor < 0)
        puts("incolor not found");
    if(attrFontUV < 0)
        puts("inuv not found");
    
    while(attrVertex < 0 || attrColor < 0 || attrFontUV < 0)
        ;
    
    #ifdef SHNPS
    uniFontTime = glGetUniformLocation(fontsh, "intime");
    uniFontNPS = glGetUniformLocation(fontsh, "innps");
    #endif
    uniFontTex = glGetUniformLocation(fontsh, "fonTex");
    
    //glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texFont);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texFont);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    void* fonttex = ctru_unpack();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_BGRA, GL_UNSIGNED_BYTE, fonttex);
    free(fonttex);
    
    glUseProgram(fontsh);
    glUniform1i(uniFontTex, 0);
    #ifdef SHNPS
    if(uniFontTime >= 0)
        glUniform1f(uniFontTime, 0);
    if(uniFontNPS >= 0)
        glUniform1f(uniFontNPS, 0);
    #endif
    glUseProgram(0);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    //glActiveTexture(0);
    //glDisable(GL_TEXTURE_2D);
    
    glDetachShader(fontsh, psh);
    glDetachShader(fontsh, vsh);
    
    glDeleteShader(vsh);
    glDeleteShader(psh);
    
    #ifdef TEXTNPS
    hist = malloc((DWORD)1e7 * 0x10);
    ZeroMemory(hist, (DWORD)1e7 * 0x10);
    midisize += (DWORD)1e7 * 0x10;
    #endif
}
#endif

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

static NoteNode* *ActiveNoteList;
static KCOLOR*   colortable;

static QWORD notealloccount;
static QWORD currnotealloc;

static struct quad* quads;
static size_t vtxidx;
static const size_t 
#ifdef _M_IX86
    vertexsize = 1 << 12;
#else
    //vertexsize = 1 << 18;
    vertexsize = 1 << 12;
#endif


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

struct histogram
{
    ULONGLONG delta;
    ULONGLONG count;
} *hist = 0;
DWORD histlast = 0;
DWORD histwrite = 0;
ULONGLONG histsum = 0;
ULONGLONG histdelay = 0;
ULONGLONG onehztimer = 0;
ULONGLONG currnote = 0;
ULONGLONG currnps = 0;

int(WINAPI*kNPOriginal)(DWORD msg);
static int WINAPI kNPIntercept(DWORD note)
{
    if(((BYTE)note & 0xF0) == 0x90 && (BYTE)(note >> 8))
        currnote++;
    
    return kNPOriginal(note);
}

static void kNPSync(MMPlayer* syncplayer, DWORD dwDelta)
{
    struct histogram* __restrict hhist = hist + (histwrite++);
    if(histwrite == (DWORD)1e7)
        histwrite = 0;
    
    hhist->delta = dwDelta * syncplayer->tempomulti;
    hhist->count = currnote;
    
    currnps += currnote;
    
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
    
    #ifdef DYNASCROLL
    return NoteSync(syncplayer, dwDelta);
    #endif
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
        //glBufferData(GL_ARRAY_BUFFER, vtxidx * sizeof(*quads),     0, GL_STREAM_DRAW);
        #ifdef GLTEXT
        drawnotesraw += vtxidx;
        #endif
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
#define NEXTFRAME ((10000000ULL / FPS_DENIM) * FPS_frame * FPS_NOMIN)
#else
#define NEXTFRAME ((10000000ULL * FPS_frame * FPS_NOMIN) / FPS_DENIM)
#endif


static void FPS_SyncFunc(MMPlayer* syncplayer, DWORD dwDelta)
{
    ULONGLONG nextframe = NEXTFRAME;
    if(nextframe <= player->RealTime)
    {
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
            if(nextframe <= player->RealTime)
            {
                puts("Frame drop");
                
                FPS_capture = TRUE;
                
                do
                {
                    INT64 sleep = -1;
                    NtDelayExecution(TRUE, &sleep);
                }
                while(FPS_capture);
                
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
    
    DWORD uid = (BYTE)(note >> 8) | (note & 0xF) << 8
    #ifdef TRACKID
    | (ply->CurrentTrack->trackid << 12)
    #endif
    ;
    
    ULONGLONG curr = TICKVAR;
    
    NoteNode* __restrict node = ActiveNoteList[uid];
    
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

#if 1
static inline void AddRawVtx(float offsy, float offst, float offsx, float offsr, KCOLOR color1)
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
    KCOLOR color = color1;
    KCOLOR color2 = color1;
    KCOLOR color3 = color;
    
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
static inline void AddRawVtx(float offsy, float offst, float offsx, float offsr, KCOLOR color1)
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
    KCOLOR color = color1;
    KCOLOR color2 = color1;
    KCOLOR color3 = color;
    
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

static inline void AddVtx(NoteNode localnode, ULONGLONG currtick, float tickscale)
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
    0xD7AFFF, //lavander
    0xFF91FB  //pink
};

const int dominolight[] =
{
    0xFFA5A5, //red
    0xFFBD55, //orang
    0xFFFF2D, //yellow
    0x8DFF55, //green
    0x55FFE2, //blue
    0xD7AFFF, //lavander
    0xFF91FB  //pink
};
#else
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

#ifdef GLTEXT
#ifdef HDR
#error HDR is not supported yet for texts
#endif
static void DrawFontString(int32_t x, int32_t y, int32_t scale, KCOLOR color, const char* text)
{
    DWORD startx = x;
    
    while(y < 72)
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
            
            struct quad* ck = quads + (vtxidx++);
            ck->quads[0] = (struct quadpart){offsx, offst, color, ux,     uy    };
            ck->quads[1] = (struct quadpart){offsx, offsy, color, ux,     uy + 8};
            ck->quads[2] = (struct quadpart){offsr, offsy, color, ux + 8, uy + 8};
            ck->quads[3] = (struct quadpart){offsr, offst, color, ux + 8, uy    };
        }
        
        x += scale;
        
        if(x >= 128)
        {
            x = startx;
            y += scale;
        }
    }
}

static DWORD notetimer = 0;

static void DrawFontOverlay()
{
    //glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texFont);
    glUseProgram(fontsh);
    
    glEnableVertexAttribArray(attrFontVertex);
    glEnableVertexAttribArray(attrFontColor);
    glEnableVertexAttribArray(attrFontUV);
    
    glVertexAttribPointer(attrFontVertex, 2, GL_FLOAT, GL_FALSE, 16, 0);
    glVertexAttribPointer(attrFontColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)8);
    glVertexAttribPointer(attrFontUV, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16, (void*)12);
    
    #ifdef SHNPS
    glUniform1f(uniFontTime, (float)((double)(player->RealTime) / 1e7));
    glUniform1f(uniFontNPS, (float)currnps);
    #endif
    
    int textlen;
    char buf[256];
    
    #ifdef TEXTNPS
    textlen = sprintf(buf, "%llu N/s ", currnps);
    DrawFontString(-textlen, 0, 2, -1, buf);
    #endif
    
    textlen = sprintf(buf, " %llu quads", drawnotes);
    DrawFontString(-textlen, -2, 2, -1, buf);
    
    if(notealloccount != currnotealloc)
    {
        currnotealloc = notealloccount;
        
        notetimer = (DWORD)2e7;
    }
    
    if(notetimer)
    {
        DWORD dwColor;
        
        if(notetimer >= (1 << 24))
            dwColor = -1;
        else
        {
            dwColor = (notetimer >> 16);
            dwColor |= dwColor << 8;
            dwColor |= dwColor << 16;
        }
        
        if(notetimer > (1 << 16))
            notetimer -= (1 << 16);
        else
            notetimer = 0;
        
        DrawFontString(-18,  9, 2, dwColor, "__________________");
        DrawFontString(-18, 10, 2, dwColor >> 1, "Memory allocation!");
        
        textlen = sprintf(buf, "%llu slots", currnotealloc);
        DrawFontString(-textlen, 6, 2, dwColor, buf);
        
        textlen = sprintf(buf, "%.3fMB slots",
                          (float)(currnotealloc * sizeof(NoteNode)) / (1024.0F * 1024.0F));
        DrawFontString(-textlen, -10, 2, dwColor, buf);
        
        textlen = sprintf(buf, "%.3fMB total",
                          (float)(midisize + (currnotealloc * sizeof(NoteNode))) / (1024.0F * 1024.0F));
        DrawFontString(-textlen, -7, 2, dwColor >> 1, buf);
    }
    
    
    glBufferData(GL_ARRAY_BUFFER, vtxidx * sizeof(*quads),     0, GL_STREAM_DRAW);
    glBufferData(GL_ARRAY_BUFFER, vtxidx * sizeof(*quads), quads, GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, vtxidx * NOTEVTX, GL_UNSIGNED_INT, 0);
    
    vtxidx = 0;
    
    glDisableVertexAttribArray(attrFontUV);
    glDisableVertexAttribArray(attrFontColor);
    glDisableVertexAttribArray(attrFontVertex);
    
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    //glActiveTexture(0);
    //glDisable(GL_TEXTURE_2D);
}
#endif

static BOOL isrender;

DWORD WINAPI RenderThread(PVOID lpParameter)
{
    puts("Hello from renderer");
    
    isrender = FALSE;
    
#ifdef TIMI_CAPTURE
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
        return GetLastError();
    }
    
    erect.left = 0;
    erect.top = 0;
    erect.bottom = 1280;
    erect.right = 720;
    
    void* capdata = malloc(1280 * 720 * 4);
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
        col = dominodark[(((i >> 4) + 6) % 7)] | (0xFF << 24);
        
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
    
    wglMakeCurrent(dc, glctx);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallbackARB(DebugCB, 0);
    
    #ifndef TIMI_CAPTURE
    uglSwapControl(1);
    #else
    uglSwapControl(0);
    #endif
    
    glViewport(0, 0, 1280, 720);
    
    CompileStandardShader();
    
    #ifdef GLTEXT
    CompileFontShader();
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
                    
                    indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 1;
                    indexes[i++] = vidx + 0;
                    indexes[i++] = vidx + 2;
                    indexes[i++] = vidx + 3;
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
    #if defined(TEXTNPS)
    kNPOriginal = player->KShortMsg;
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
    ULONGLONG timersync = ~0;
    timersync = timersync >> 2;
    
    player->SyncPtr = (LONGLONG*)&timersync;
    player->SyncOffset = 0;
    
    player->KShortMsg = FPS_ShortMsg;
    player->KSyncFunc = FPS_SyncFunc;
    
    ply->SleepTicks = 1;
    ply->SleepTimeMax = 0;
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
    
    while(!(WaitForSingleObject(vsyncevent, timeout) >> 9))
    {
        if(canrender && !isrender)
        {
            CreateThread(0, 0x4000, PlayerThread, player, 0, 0);
            
            isrender = TRUE;
            
            #ifdef TIMI_CAPTURE
            
            player->SyncPtr = (LONGLONG*)&timersync;
            player->SyncOffset = 0;
            
            player->KShortMsg = FPS_ShortMsg;
            player->KSyncFunc = FPS_SyncFunc;
            #endif
        }
        
        ResetEvent(vsyncevent);
        
        
        glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        //glClearDepth(1.0);
        //glClearColor(0.5859375F, 0.5859375F, 0.5859375F, 0.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if(glBindVertexArray)
            glBindVertexArray(g_vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferData(GL_ARRAY_BUFFER, datasize, 0, GL_STREAM_DRAW);
        
        glEnableVertexAttribArray(attrVertex);
        glEnableVertexAttribArray(attrColor);
        
        glVertexAttribPointer(attrVertex, 2, GL_FLOAT, GL_FALSE, sizeof(struct quadpart), 0);
        #ifndef HDR
        glVertexAttribPointer(attrColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct quadpart), (void*)8);
        #else
        glVertexAttribPointer(attrColor, 4, GL_FLOAT, GL_FALSE, sizeof(struct quadpart), (void*)8);
        #endif
        
        glUseProgram(sh);
        if(glBindVertexArray)
            glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
        
        #if defined(HDR) && defined(TRIPPY)
        glUniform1f(attrNotemix, 1);
        #endif
        
        #ifdef GLTEXT
        drawnotesraw = 0;
        #endif
        
        #ifdef DYNASCROLL
        ULONGLONG notesync = mmnotesync;
        ULONGLONG currtick = syncvalue;
        #else
        ULONGLONG notesync = TICKVAR;
        ULONGLONG currtick = TICKVAL;
        DWORD tickheight = (2500000
        #if defined(TRANSFORM) || defined(ROTAT)
        * 4
        #endif
        ) / player->tempomulti;
        float tickscale = 1.0F / (float)tickheight;
        #endif
        
        #ifdef KEYBOARD
        currtimer = player->RealTime;
        #endif
        
        #ifdef SHTIME
        #ifdef DYNASCROLL
        glUniform1f(uniTime, (float)(currtick / (double)tickheight * 0.25));
        #else
        glUniform1f(uniTime, (float)((double)(player->RealTime) / 1e7));
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
        
        /*#ifdef HDR
        #ifdef TRIPPY
        const float uc = 0.0F;
        const float kc = -1.5F;
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
        AddRawVtx(kc, 1, 0, 600, (KCOLOR){uc, uc, uc, 1});
        #endif
        #endif
        */
        
        #if defined(HDR) && defined(TRIPPY)
        if(vtxidx)
            FlushToilet();
        
        glUniform1f(attrNotemix, 1);
        #endif
        
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
        }
        
        if(!freelist_return)
        {
            freelist_returnhead = freelist_head;
            freelist_head = 0;
            freelist_return = freelist;
            freelist = 0;
        }
        
        if(vtxidx)
            FlushToilet();
        
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
        
        #ifdef GLTEXT
        drawnotes = drawnotesraw + vtxidx;
        #endif
        
        #ifdef KEYBOARD
        
        #if defined(HDR) && defined(TRIPPY)
        glUniform1f(attrNotemix, 0);
        //glUniform1f(attrNotemix, 1.0F / 64.0F);
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
        glUniform1fv(uniLightAlpha, 256, shalpha);
        glUniform4fv(uniLightColor, 256, (GLfloat*)shcolor);
        #else
        glUniform1fv(uniLightAlpha, 128, shalpha);
        glUniform4fv(uniLightColor, 128, (GLfloat*)shcolor);
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
        
        glDisableVertexAttribArray(attrVertex);
        glDisableVertexAttribArray(attrColor);
        
        #ifdef GLTEXT
        DrawFontOverlay();
        #endif
        
        glUseProgram(0);
        //glFlush();
        
        InvalidateRect(glwnd, 0, 0);
        uglSwapBuffers(dc);
        
        glViewport(erect.left, erect.top, erect.right - erect.left, erect.bottom - erect.top);
        
        NtQuerySystemTime(&currtime);
        #ifndef TIMI_CAPTURE
        if((currtime - prevtime) >> 18)
            timeout = 0;
        else
            timeout = 30;
        #endif
        prevtime = currtime;
        
        #ifdef TIMI_CAPTURE
        if(FPS_capture)
        {
            FPS_capture = FALSE;
            
            glReadPixels(
                0, 0, 1280, 720,
                GL_BGRA, GL_UNSIGNED_BYTE,
                capdata
            );
            
            DWORD written;
            while(!WriteFile(hpipe, capdata, 1280 * 720 * 4, &written, 0))
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
        }
        #endif
        
        //#ifndef KEYBOARD
        if(player->tracks->ptrs)
            continue;
        
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
    
#ifdef TIMI_CAPTURE
    CloseHandle(hpipe);
    PostQuitMessage(1);
#endif
    
    wglMakeCurrent(0, 0);
    
    puts("Renderer died");
    
    return 0;
}
