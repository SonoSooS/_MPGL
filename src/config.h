#pragma once

// ========[Main config]========

//#define CUSTOMTITLE L"render"
//#define TRIPLEO
//#define NOBUF
//#define FULLSCREEN
#define MIDI_MMIO
//#define HEADLESS
//#define NO_ONEKEY
#define NO_ZEROKEY

//#define MODE_BH

// ========[Renderer config]========

#define TRACKID
#define PFACOLOR
#define PFAKEY
//#define MMVKEY
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
#define SHTIME
//#define WOBBLE
//#define WOBBLE_INTERP
//#define GLOWEDGE
#define GLTEXT
#define TEXTNPS
#define SHNPS
//#define SHWOBBLE
//#define HDR
//#define NOKEYBOARD
//#define TIMI_CAPTURE
//#define TIMI_NOCAPTURE
//#define TIMI_IMPRECISE
//#define TIMI_SILENT
//#define TIMI_NOWAIT
//#define TIMI_CUSTOMSCROLL
//#define FASTHDR
//#define TEXTALLOC
#define TEXTNEAT
#define TEXTCUSTOM1
#define TEXTTRANS
#define GRACE
//#define DEBUGTEXT
//#define EXTREMEDEBUG
#define OLDDENSE
//#define NOISEOVERLAY
//#define BUGFIXTEST
//#define NORENDEROPT

#define WMA_SIZE 16
//#define WMA_SIZE 3

//#define CUSTOMTICK PlayerReal->timediv >> 3
//#define CUSTOMTICK PlayerReal->timediv >> 2
#define CUSTOMTICK   PlayerReal->timediv

#define TIMI_FPS_DENIM 60
#define TIMI_FPS_NOMIN 1

#define CAPW 1920
#define CAPH 1080

#define BLITMODE GL_NEAREST
//#define BLITMODE GL_LINEAR



#ifdef ROUNDEDGE
#define NOTEVTX 36
#define QUADCOUNT 9
#else
#define NOTEVTX 6
#define QUADCOUNT 4
#endif

