#pragma once

#include <wtypes.h>

typedef ULONGLONG QWORD; //why what fuck

typedef struct MMTrack
{
    BYTE* ptrs;
    BYTE* ptre;
    QWORD nextcounter;
    DWORD trackid;
    BYTE cmd, prm1, prm2, sbz;
    
} MMTrack;

typedef struct MMPlayer
{
    QWORD TickCounter;
    LONGLONG RealTime;
    DWORD SleepTimeMax;
    DWORD SleepTicks;
    MMTrack* tracks;
    MMTrack* CurrentTrack;
    DWORD flags;
    DWORD tempo;
    DWORD tempomulti;
    WORD timediv;
    WORD done;
    int(WINAPI*KShortMsg)(DWORD msg);
    int(WINAPI*KLongMsg)(DWORD msg, LPCVOID buf, DWORD len);
    void(*KSyncFunc)(struct MMPlayer* player, DWORD dwDelta);
    LONGLONG* SyncPtr;
    LONGLONG SyncOffset;
    LONGLONG RealTimeUndiv;
} MMPlayer;

MMPlayer* mmpDuplicatePlayer(const MMPlayer* other);
DWORD WINAPI PlayerThread(PVOID lpParameter);
