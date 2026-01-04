#pragma once

#include <wtypes.h>
#include "types.h"

typedef u64 MMTick;

typedef union MMEvent MMEvent;
typedef struct MMTrack MMTrack;
typedef struct MMPlayer MMPlayer;

typedef int(WINAPI*cbMMShortMsg)(DWORD msg);
typedef int(WINAPI*cbMMLongMsg)(DWORD msg, LPCVOID buf, DWORD len);
typedef void(WINAPI*cbMMSync)(MMPlayer* player, DWORD deltaTicks);

struct MMTimer
{
    MMTick previous_current_time;
    MMTick previous_wanted_elapsed_time;
    s64 sleep_offset_error;
    s64 max_offset_error;
};



union MMEvent
{
    struct
    {
        u8 cmd;
        u8 prm1;
        u8 prm2;
        u8 _sbz;
    };
    u32 dwEvent;
};

struct MMTrack
{
    u8* ptrs;
    u8* ptre;
    MMTick nextcounter;
    u32 trackid;
    MMEvent event;
};

struct MMPlayer
{
    MMTick TickCounter;
    s64 RealTime;
    u32 SleepTicksMax;
    u32 TrackCount;
    MMTrack* tracks;
    MMTrack* CurrentTrack;
    u32 tempo;
    u32 timediv;
    bool done;
    cbMMShortMsg KShortMsg;
    cbMMLongMsg KLongMsg;
    cbMMSync KSyncFunc;
    s64* SyncPtr;
    s64 SyncOffset;
    s64 RealTimeUndiv;
    struct MMTimer timer;
};

MMPlayer* mmpDuplicatePlayer(const MMPlayer* other);
DWORD WINAPI PlayerThread(PVOID lpParameter);
