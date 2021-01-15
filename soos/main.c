#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GL/ctor.h"
#include "GL/core.h"
#include "GL/utility.h"

#include <GL/wglext.h>

#include "player/mmplayer.h"

extern DWORD (WINAPI*mGetModuleBaseNameA)
(
    HANDLE  hProcess,
    HMODULE hModule,
    LPSTR   lpBaseName,
    DWORD   nSize
);

//#define CUSTOMTITLE L"PFA mode"
//#define CUSTOMTITLE L"render"
#define CUSTOMTITLE L"no antispam"
//#define TRIPLEO
//#define NOBUF
//#define FULLSCREEN
// TIMI_TIEMR was moved to render.c

#include "exch.h"

const LPCWSTR WClassName = L"MPGL_FormMain";
const DWORD windowstyle = WS_OVERLAPPED | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_GROUP | WS_TABSTOP;
const DWORD windowexstyle = WS_EX_WINDOWEDGE | WS_EX_APPWINDOW;

HWND glwnd;
HDC dc;
HGLRC glctx;
HANDLE vsyncevent;
RECT erect;

BOOL canrender;

HMODULE KSModule;

static MMPlayer* realplayer;
static ULONGLONG realsync = 0;

DWORD WINAPI RenderThread(PVOID lpParameter);

static HGLRC CreateGLContext(HWND wnd, HDC dc)
{
    HWND tempwnd = CreateWindowExW(windowexstyle, L"STATIC", L"MPGL Test Window " DATETIME, windowstyle, 0, 0, 1, 1, 0, 0, GetModuleHandleA(0), 0);
    
    if(!tempwnd)
    {
        puts("No tempwnd");
        return 0;
    }
    
    HDC wdc = GetDC(tempwnd);
    if(!wdc)
    {
        puts("No WDC");
        return 0;
    }
    
    PIXELFORMATDESCRIPTOR pfd;/* =
    {
    	sizeof(PIXELFORMATDESCRIPTOR),
    	1,
    	PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_SUPPORT_COMPOSITION, // Flags
    	PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
    	32,                   // Colordepth of the framebuffer.
    	0, 0, 0, 0, 0, 0,
        8, 0,
    	0, 0, 0, 0, 0,
    	16,                   // Number of bits for the depthbuffer
    	16,                    // Number of bits for the stencilbuffer
    	2,                    // Number of Aux buffers in the framebuffer.
    	PFD_MAIN_PLANE,
    	0,
    	0, 0, 0
    };*/
    
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SUPPORT_COMPOSITION | PFD_SWAP_EXCHANGE;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cRedBits = 8;
    pfd.cRedShift = 16;
    pfd.cGreenBits = 8;
    pfd.cGreenShift = 8;
    pfd.cBlueBits = 8;
    pfd.cBlueShift = 0;
    pfd.cAlphaBits = 8;
    pfd.cAlphaShift = 24;
    pfd.cAccumBits = 64;
    pfd.cAccumRedBits = 16;
    pfd.cAccumGreenBits = 16;
    pfd.cAccumBlueBits = 16;
    pfd.cAccumAlphaBits = 16;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 16;
    pfd.cAuxBuffers = 4;
    pfd.iLayerType = PFD_MAIN_PLANE;
    
    int pixfmt = ChoosePixelFormat(wdc, &pfd);
    int pixfmt2 = -1;
    
    if(!pixfmt)
        puts("LWJGLException: Pixel format not accelerated");
    
    if(!SetPixelFormat(wdc, pixfmt, &pfd))
        puts("Failed to set pixel format");
    
    HGLRC ctx = wglCreateContext(wdc);
    if(!ctx)
    {
        puts("Failed to create temporary context");
        DestroyWindow(tempwnd);
        return 0;
    }
    
    wglMakeCurrent(wdc, ctx);
    
    GL_LinkFunctions();
    
    if(uglSupportsExt("WGL_ARB_pixel_format"))
    {
        puts("Has WGL_ARB_pixel_format");
        
        BOOL(WINAPI*wglChoosePixelFormat)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats) = (void*)GL_GetProcAddress("wglChoosePixelFormatARB");
        BOOL(WINAPI*wglGetPixelFormatAttribivARB)(HDC hdc, int iPixelFormat, int iLayerType, int nAttributes, const int* piAttributes, int* piValues) = (void*)GL_GetProcAddress("wglGetPixelFormatAttribivARB");
        if(wglChoosePixelFormat && wglGetPixelFormatAttribivARB)
        {
            puts("Trying wglChoosePixelFormatARB");
            
            int params[] =
            {
                //WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_EXT,
                //WGL_DRAW_TO_WINDOW_EXT, GL_TRUE,
                /*WGL_RED_BITS_EXT, 8,
                WGL_GREEN_BITS_EXT, 8,
                WGL_BLUE_BITS_EXT, 8,
                WGL_ALPHA_BITS_EXT, 8,*/
                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_FLOAT_ARB,
                //WGL_DEPTH_BITS_EXT, 16,
                WGL_DOUBLE_BUFFER_EXT, GL_TRUE,
                0, 0
            };
            UINT succ = 0;
            if(wglChoosePixelFormat(wdc, params, 0, 1, &pixfmt2, &succ) && succ && pixfmt)
            {
                printf("Found new pixel format, replacing %i with %i\n", pixfmt, pixfmt2);
                
                int origfmt = pixfmt;
                pixfmt = pixfmt2;
                pixfmt2 = origfmt;
                
                /*
                int paramin[] =
                {
                    WGL_ACCELERATION_EXT,
                    WGL_RED_BITS_EXT,
                    WGL_GREEN_BITS_EXT,
                    WGL_BLUE_BITS_EXT,
                    WGL_ALPHA_BITS_EXT,
                    WGL_COLOR_BITS_EXT,
                    WGL_DEPTH_BITS_EXT,
                    WGL_STENCIL_BITS_EXT,
                    WGL_SAMPLE_BUFFERS_ARB,
                    WGL_SAMPLES_ARB,
                    WGL_ACCUM_RED_BITS_ARB,
                    WGL_ACCUM_GREEN_BITS_ARB,
                    WGL_ACCUM_BLUE_BITS_ARB,
                    WGL_ACCUM_ALPHA_BITS_ARB,
                    WGL_ACCUM_BITS_ARB,
                    WGL_DOUBLE_BUFFER_ARB,
                    WGL_STEREO_ARB,
                    0
                };
                
                int paramout[sizeof(paramin)/sizeof(*paramin)];
                
                if(wglGetPixelFormatAttribivARB(wdc, pixfmt2, 0, (sizeof(paramin)/sizeof(*paramin)) - 1, paramin, paramout))
                {
                    pixfmt = pixfmt2;
                    
                }
                else puts("Failed to get pixel format attributes");*/
            }
            else puts("Failed to find a better pixel format");
        }
    }
    
    
    
    if(!DescribePixelFormat(dc, pixfmt, sizeof(pfd), &pfd))
    {
        printf("Failed to describe PixelFormat %i\n", pixfmt);
        
        if(pixfmt2 != -1 && !DescribePixelFormat(dc, pixfmt2, sizeof(pfd), &pfd))
        {
            printf("Failed to describe fallback PixelFormat %i\n", pixfmt2);
            goto ctxfail;
        }
        else
        {
            pixfmt = pixfmt2;
        }
    }
    
    if(!SetPixelFormat(dc, pixfmt, &pfd))
    {
        puts("Failed to set actual pixel format");
        goto ctxfail;
    }
    
    HGLRC(WINAPI*glCreateContextAttribsARB)(HDC dc, PVOID share, const int* params) = (void*)GL_GetProcAddress("wglCreateContextAttribsARB");
    if(glCreateContextAttribsARB)
    {
        puts("Using custom attrs");
        
        int attrs[] =
        {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            0, 0
        };
        HGLRC ct = glCreateContextAttribsARB(dc, 0, attrs);
        if(ct)
        {
            wglMakeCurrent(wdc, 0);
            wglMakeCurrent(0, 0);
            
            wglDeleteContext(ctx);
            DestroyWindow(tempwnd);
            return ct;
        }
    }
    
    wglMakeCurrent(wdc, 0);
    wglMakeCurrent(0, 0);
    
    wglDeleteContext(ctx);
    DestroyWindow(tempwnd);
    
    return wglCreateContext(dc);
    
    ctxfail:
    
    do
    {
        char errbuf[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errbuf, 255, NULL);
        errbuf[255] = 0;
        puts(errbuf);
    }
    while(0);
    
    wglMakeCurrent(wdc, 0);
    wglMakeCurrent(0, 0);
    
    wglDeleteContext(ctx);
    DestroyWindow(tempwnd);
    
    return 0;
}

static LRESULT CALLBACK WindowProc(HWND wnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_CREATE:
        {
            if(!((LPCREATESTRUCTW)lParam)->lpCreateParams)
                break;
            
            dc = GetDC(wnd);
            
            glctx = CreateGLContext(wnd, dc);
            if(!glctx)
            {
                puts("Failed to create GL context");
                return -1;
            }
            
            vsyncevent = CreateEventA(0, TRUE, FALSE, 0);
            
            LRESULT cwresult = DefWindowProcW(wnd, uMsg, wParam, lParam);
            
            //fuck lose10 for breaking a such simple thing as AdjustWindowRect ª_ª
            
            RECT wndrect;
            RECT clirect;
            if(GetWindowRect(wnd, &wndrect) && GetClientRect(wnd, &clirect))
            {
                /*
                WND:  320; 180 - 1600; 900
                CLI:    0;   0 - 1264; 681
                ADJ:  304; 141 - 1296; 759
                ACT:           - 1282; 752
                TGT:           - 
                */
                
                //printf("WND: %4i;%4i - %4i;%4i\n", wndrect.left, wndrect.top, wndrect.right, wndrect.bottom);
                //printf("CLI: %4i;%4i - %4i;%4i\n", clirect.left, clirect.top, clirect.right, clirect.bottom);
                
                LONG width = wndrect.right - wndrect.left; // 1280
                LONG height = wndrect.bottom - wndrect.top; // 720
                
                RECT calc;
                calc.left = wndrect.left - ((width - clirect.right) >> 1); //kinda works
                calc.top = wndrect.top - ((height - clirect.bottom) >> 1); //close enough
                calc.right = (width * 2) - clirect.right;
                calc.bottom = (height * 2) - clirect.bottom;
                
                //printf("ADJ: %4i;%4i - %4i;%4i\n", calc.left, calc.top, calc.right, calc.bottom);
                
                SetWindowPos(wnd, 0, calc.left, calc.top, calc.right, calc.bottom, SWP_FRAMECHANGED);
            }
            
            CreateThread(0, 0x4000, RenderThread, ((LPCREATESTRUCTW)lParam)->lpCreateParams, 0, 0);
            
            return cwresult;
        }

    #ifdef FULLSCREEN
        case WM_SHOWWINDOW:
        {
            LRESULT cwresult = DefWindowProcW(wnd, uMsg, wParam, lParam);
            
            DEVMODEW screen;
            EnumDisplaySettingsW(0, ENUM_CURRENT_SETTINGS, &screen);
            
            SetWindowLong(wnd, GWL_STYLE, WS_VISIBLE);
            SetWindowLong(wnd, GWL_EXSTYLE, 
                WS_EX_APPWINDOW | WS_EX_TOPMOST);
            SetWindowPos(wnd, 0, screen.dmPosition.x, screen.dmPosition.y,
                screen.dmPosition.x + screen.dmPelsWidth, screen.dmPosition.y + screen.dmPelsHeight,
                SWP_FRAMECHANGED);
            
            screen.dmFields = 0;
            ChangeDisplaySettingsW(&screen, CDS_FULLSCREEN);
            
            return cwresult;
        }
        
        case WM_SETCURSOR:
        {
            if(LOWORD(lParam) == HTCLIENT)
            {
                SetCursor(0);
                return TRUE;
            }
            
            break;
        }
    #else
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            if(wParam == VK_RETURN && (HIWORD(lParam) & KF_ALTDOWN))
            {
                DEVMODEW screen;
                EnumDisplaySettingsW(0, ENUM_CURRENT_SETTINGS, &screen);
                
                SetWindowLong(wnd, GWL_STYLE, WS_VISIBLE);
                //SetWindowLong(wnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
                SetWindowPos(wnd, 0, screen.dmPosition.x, screen.dmPosition.y,
                    screen.dmPosition.x + screen.dmPelsWidth, screen.dmPosition.y + screen.dmPelsHeight,
                    SWP_FRAMECHANGED);
            }
            else if(wParam == VK_SPACE && !(HIWORD(lParam) & KF_ALTDOWN))
            {
                if(!canrender)
                    canrender = TRUE;
                else if(realplayer->SyncPtr)
                    realplayer->SyncPtr = 0;
                else
                {
                    realsync = -realplayer->SyncOffset;
                    realplayer->SyncPtr = (LONGLONG*)&realsync;
                }
            }
            
            break;
        }
    #endif
        
        case WM_PAINT:
        {
            SetEvent(vsyncevent);
            //return 0;
            break;
        }
        
        case WM_SIZE:
        {
            LRESULT cwresult = DefWindowProcW(wnd, uMsg, wParam, lParam);
            
            GetClientRect(wnd, &erect);
            
            return cwresult;
        }
        
        case WM_ERASEBKGND:
        {
            //return 0;
            break;
        }
        
        case WM_CLOSE:
        case WM_DESTROY:
        {
            CloseHandle(vsyncevent);
            vsyncevent = 0;
            
            wglMakeCurrent(0, 0);
            wglDeleteContext(glctx);
            glctx = 0;
            
            PostQuitMessage(0);
            
            break;
        }
    }
    
    return DefWindowProcW(wnd, uMsg, wParam, lParam);
}

#ifdef TRIPLEO

static int dwNoMessage(DWORD dwParam1)
{
    return 0;
}

#endif

QWORD midisize;

static MMPlayer* CreatePlayer(LPCWCH testpath)
{
    OPENFILENAME_NT4W ofd;
    ZeroMemory(&ofd, sizeof(ofd));
    
    WCHAR filter[64];
    wcsncpy(filter, L"MIDI Files (*.mid)\0*.mid\0", 64);
    WCHAR path[262];
    //ZeroMemory(path, sizeof(path));
    wcsncpy(path, L"*.mid", 262);
    
    if(!testpath)
    {
        ofd.hInstance = GetModuleHandleA(0);
        ofd.lpstrFilter = filter;
        ofd.lpstrTitle = L"Open a file";
        ofd.lStructSize = sizeof(ofd);
        ofd.nMaxFile = 262;
        ofd.nFilterIndex = 1;
        ofd.lpstrFile = path;
        ofd.Flags = OFN_FILEMUSTEXIST;
        
        if(!GetOpenFileNameW((LPOPENFILENAMEW)&ofd))
            return 0;
        
        testpath = path;
    }
    
    printf("%S\n", testpath);
    
    HANDLE f = CreateFileW(testpath, GENERIC_READ, 0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if(!f)
    {
        puts("File open fail");
        return 0;
    }
    
    if(GetLastError())
        printf("ERR=%i\n", GetLastError());
    
    MMPlayer* player = malloc(sizeof(MMPlayer));
    if(!player)
    {
        CloseHandle(f);
        puts("Failed to create MMPlayer");
        return 0;
    }
    ZeroMemory(player, sizeof(*player));
    
    ULARGE_INTEGER fs;
    fs.LowPart = 0;
    fs.HighPart = 0;
    
    fs.LowPart = GetFileSize(f, &fs.HighPart);
    
    if(GetLastError() ||
    #ifndef _WIN64
    fs.HighPart ||
    #endif
    (!fs.HighPart && fs.LowPart < 0x19))
    {
        CloseHandle(f);
        printf("ERR=%i Low=%08X High=%08X\n", GetLastError(), fs.LowPart, fs.HighPart);
        puts("Invalid file size");
        return 0;
    }
    
    midisize = fs.QuadPart;
    
    BYTE* ptr = malloc(
    #ifndef _WIN64
    fs.LowPart
    #else
    fs.QuadPart
    #endif
    );
    
    if(!ptr)
    {
        CloseHandle(f);
        puts("Memory allocation failure");
        goto unloadfail;
        //return 1;
    }
    BYTE* gptr = ptr;
    BYTE* dstptr = ptr;
    
    while(
    #ifndef _WIN64
    fs.LowPart
    #else
    fs.QuadPart
    #endif
    )
    {
        printf("Reading %llX...        \r", fs.QuadPart);
        
        DWORD mustread = (fs.HighPart || (fs.LowPart >> 30)) ? 0x40000000 : fs.LowPart;
        if(!ReadFile(f, dstptr, mustread, &mustread, NULL))
        {
            puts("File read failure");
            goto unloadfail;
        }
        
    #ifndef _WIN64
        fs.LowPart
    #else
        fs.QuadPart
    #endif
        -= mustread;
        dstptr += mustread;
    }
    
    CloseHandle(f);
    
    BYTE* ptrend = dstptr;
    
    if(*(DWORD*)ptr != *(DWORD*)"MThd")
    {
        puts("No MThd");
        return 0;
    }
    
    DWORD trkcnt = __builtin_bswap32(*(DWORD*)(ptr + 4));
    if(trkcnt < 6)
    {
        puts("Invalid MThd chunk size");
        return 0;
    }
    
    if(__builtin_bswap16(*(WORD*)(ptr + 8)) > 1)
    {
        puts("Unsupported MIDI version");
        return 0;
    }
    
    WORD timediv = __builtin_bswap16(*(WORD*)(ptr + 0xA));
    
    BYTE* buf = ptr + trkcnt + 8;
    trkcnt = 0;
    
    while(buf < ptrend)
    {
        if(!timediv) break;
        
        if(*(DWORD*)buf == *(DWORD*)"MTrk")
        {
            trkcnt++;
            timediv--;
        }
        
        buf += __builtin_bswap32(*(DWORD*)(buf + 4));
        buf += 8;
    }
    
    player->tracks = calloc(trkcnt + 1, sizeof(MMTrack));
    ZeroMemory(player->tracks, sizeof(MMTrack) * (trkcnt + 1));
    
    buf = ptr + __builtin_bswap32(*(DWORD*)(ptr + 4)) + 8;
    timediv = 0;
    while(buf < ptrend)
    {
        if(trkcnt == timediv) break;
        
        if(*(DWORD*)buf == *(DWORD*)"MTrk")
        {
            player->tracks[timediv].ptrs = buf + 8;
            
            buf += __builtin_bswap32(*(DWORD*)(buf + 4));
            buf += 8;
            
            player->tracks[timediv].ptre = buf;
            player->tracks[timediv].trackid = timediv;
            
            timediv++;
        }
        else
        {
            buf += __builtin_bswap32(*(DWORD*)(buf + 4));
            buf += 8;
        }
    }
    
    player->timediv = __builtin_bswap16(*(WORD*)(ptr + 0xC));
    
    player->tempo = 500000;
    
    player->tempomulti = (player->tempo * 10) / player->timediv;
    
    return player;
    
    unloadfail:
    
    //free some memoery
    
    return 0;
}

int main(int argc, char** argv)
{
#ifdef _M_IX86
    //*((HANDLE*)((BYTE*)(((BYTE*)NtCurrentTeb()) + 0x30) + 0x18)) = HeapCreate(0, 0, 0);
#endif
    
    SetUnhandledExceptionFilter(crashhandler);
    //AddVectoredExceptionHandler(1, crashhandler);
    
    mGetModuleBaseNameA = (void*)GetProcAddress(GetModuleHandleA("psapi"), "GetModuleBaseNameA");
    if(!mGetModuleBaseNameA)
        mGetModuleBaseNameA = (void*)GetProcAddress(GetModuleHandleA("kernel32"), "GetModuleBaseNameA");
    if(!mGetModuleBaseNameA)
        mGetModuleBaseNameA = (void*)GetProcAddress(GetModuleHandleA("kernel32"), "K32GetModuleBaseNameA");
    
    if(!mGetModuleBaseNameA)
    {
        puts("Trying psapi");
        mGetModuleBaseNameA = (void*)GetProcAddress(LoadLibraryA("psapi"), "GetModuleBaseNameA");
    }
    
    if(!mGetModuleBaseNameA)
        puts("Warning: recursive crashing ahead!");
    
    puts("MIDI Player OpenGL");
    puts(" build " DATETIME);
    puts("Copyright (C) 2019-2020 Sono");
    puts("All Rights Reserved.");
    puts("");
    
#ifndef TRIPLEO
    HMODULE ks = LoadLibraryA(".\\timiditydrv");
    if(ks)
    {
        if(!GetProcAddress(ks, "IsKDMAPIAvailable")
        || !GetProcAddress(ks, "InitializeKDMAPIStream")
        || !GetProcAddress(ks, "SendDirectData"
    #ifdef NOBUF
        "NoBuf"
    #endif
        )
        || !GetProcAddress(ks, "TerminateKDMAPIStream"))
        {
            FreeLibrary(ks);
            ks = 0;
        }
    }
    
    if(!ks)
        ks = LoadLibraryA("syndrv.dll");
    
    if(!ks)
    {
        puts("Loading OmniMIDI");
        ks = LoadLibraryA("OmniMIDI\\OmniMIDI.dll");
    }
    
    if(!ks)
    {
        puts("Can't load OmniMIDI");
        return 1;
    }
    
    void(WINAPI*KDLayers)(DWORD) = (void*)GetProcAddress(ks, "syninit_SetLayersKDM");
    if(KDLayers)
    {
        puts("Found layers function");
        
        FILE* f = fopen("layers.bin", "rb");
        if(f)
        {
            puts("Reading layers");
            
            DWORD layers = 4;
            fread(&layers, 1, 4, f);
            fclose(f);
            
            printf("Layers for syndrv: %i\n", layers);
            KDLayers(layers);
        }
    }
    
    KSModule = ks;
#else
    KSModule = 0;
#endif
    
    MMPlayer* player = 0;
    
    int wcargc = 0;
    wchar_t** wcargv = 0;
    if(1)
    {
        wchar_t* cmdline = GetCommandLineW();
        wchar_t**(WINAPI*argparse)(wchar_t* cmdline, int* outcnt) = 0;
        HMODULE shl = GetModuleHandleA("shell32");
        if(!shl) shl = LoadLibraryA("shell32");
        if(shl)
        {
            argparse = (void*)GetProcAddress(shl, "CommandLineToArgvW");
            if(argparse)
                wcargv = argparse(cmdline, &wcargc);
        }
        if(!argparse || wcargc < 2)
        {
            puts("Showing OpenFileDialog");
            player = CreatePlayer(0);
        }
        else
        {
            player = CreatePlayer(wcargv[1]);
        }
    }
    
    if(!player)
    {
#ifndef TRIPLEO
        FreeLibrary(ks);
#endif
        puts("Failed to open file");
        
        return 1;
    }
    
    MMPlayer* renderplayer = mmpDuplicatePlayer(player);
    if(!renderplayer)
    {
#ifndef TRIPLEO
        FreeLibrary(ks);
#endif
        puts("Failed to create note catcher due to low memory");
        
        return 1;
    }
    
#ifndef TRIPLEO
    do
    {
        puts("Checking for KDMAPI");
        
        BOOL(WINAPI*KSInit)(void) = (void*)GetProcAddress(ks, "IsKDMAPIAvailable");
        if(!KSInit)
        {
            FreeLibrary(ks);
            puts("Missing KDMAPI check");
            return 1;
        }
        
        puts("KDMAPI test");
        
        if(!KSInit())
        {
            FreeLibrary(ks);
            puts("KDMAPI be like \"no u\". Please enable KDMAPI from the configurator");
            return 1;
        }
        
        puts("Checking for KDMAPI init");
        
        KSInit = (void*)GetProcAddress(ks, "InitializeKDMAPIStream");
        if(!KSInit)
        {
            FreeLibrary(ks);
            puts("Missing KDMAPI");
            return 1;
        }
        
        puts("KDMAPI init");
        
        if(!KSInit())
        {
            FreeLibrary(ks);
            puts("KDMAPI be like \"fuck you\"");
            return 1;
        }
        
        
        player->KShortMsg = (void*)GetProcAddress(ks, "SendDirectData"
    #ifdef NOBUF
        "NoBuf"
    #endif
        );
        
    #ifdef NOBUF
        LONGLONG delay = -2500000;
        ((BOOL(WINAPI*)(BOOL,PLONGLONG))
            GetProcAddress(GetModuleHandleA("ntdll"),"NtDelayExecution"))(0, &delay);
    #endif
    }
    while(0);
#else
    player->KShortMsg = dwNoMessage;
#endif
    
    erect.left = 0;
    erect.top = 0;
    erect.right = 1280;
    erect.bottom = 720;
    
    WNDCLASSW wndclass;
    ZeroMemory(&wndclass, sizeof(wndclass));
    wndclass.style = CS_OWNDC;
    wndclass.lpfnWndProc = WindowProc;
    wndclass.hInstance = GetModuleHandleA(0);
#ifndef FULLSCREEN
    //wndclass.hCursor = LoadCursorA(0, IDC_CROSS);
#endif
    wndclass.hbrBackground = GetStockObject(BLACK_BRUSH);
    wndclass.lpszClassName = WClassName;
    
    RegisterClassW(&wndclass);
    
    RECT rekt;
    rekt.left = 0;
    rekt.top = 0;
    rekt.right = GetSystemMetrics(SM_CXSCREEN);
    rekt.bottom = GetSystemMetrics(SM_CYSCREEN);
    
    if(rekt.right >= 1280 && rekt.bottom >= 720)
    {
        rekt.left = (rekt.right - 1280) >> 1;
        rekt.top = (rekt.bottom - 720) >> 1;
        rekt.right = 1280;
        rekt.bottom = 720;
    }
    
    canrender = FALSE;
    
    realplayer = player;
    
    MMPlayer* wndparam[2];
    wndparam[0] = renderplayer;
    wndparam[1] = player;
    
    glwnd = CreateWindowExW
    (
        windowexstyle,
        WClassName,
        L"MIDIPlayer OpenGL"
#ifdef CUSTOMTITLE
        L" (" CUSTOMTITLE L")"
#endif
#ifdef TRIPLEO
        L" (ooo000 edition)"
#endif
        ,
        windowstyle,
        rekt.left,
        rekt.top,
        rekt.right,
        rekt.bottom,
        0,
        0,
        wndclass.hInstance,
        wndparam
    );
    
    if(!glwnd)
    {
        puts("No WND");
        return 1;
    }
    
    puts("Showing FormMain");
    
    ShowWindow(glwnd, SW_SHOWNORMAL);
    
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    
    while((int)GetMessageW(&msg, 0, 0, 0) > 0)
    {
        DispatchMessageW(&msg);
    }
    
    puts("GetMessage loop broken");
    
#ifndef TRIPLEO
    do
    {
        void(WINAPI*KSDeinit)(void) = (void*)GetProcAddress(ks, "TerminateKDMAPIStream");
        if(!KSDeinit)
            puts("How the fuck did you even get here?!");
        else
            KSDeinit();
        FreeLibrary(ks);
    }
    while(0);
#endif
        
    fflush(stdout);
    
    return 0;
}
