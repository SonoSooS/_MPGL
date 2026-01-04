#include <windows.h>
#include <stdio.h>

#include "config.h"
#include "mmplayer.h"

#ifdef MODE_BH
#include "bh.h"
#endif

#define MMIDI_TIMER_MAX_ERROR 10000000 /* 1s */
#define MMIDI_TIMER_DEFAULT_ERROR 200000 /* 20ms */

static MMTick mmtimer_get(struct MMTimer* __restrict timer, MMTick current_time, MMTick want_delta_time)
{
    MMTick elapsed_real_time = current_time - timer->previous_current_time;
    timer->previous_current_time = current_time;
    
    s64 sleep_offset_error;
    {
        /*
            Allow plus minus derivation for faulty time implementations
            (for example broken HPET, or DST switch).
            
            Also cap the maximum correctable error.
            Outside of the correction range will be handled by the host.
        */
        MMTick error_checking_term = elapsed_real_time + MMIDI_TIMER_MAX_ERROR;
        if((error_checking_term >= 0) && (error_checking_term < (2ULL * MMIDI_TIMER_MAX_ERROR))) // Prevent overflows
        {
            s64 time_offset_error = (s64)(elapsed_real_time - timer->previous_wanted_elapsed_time);
            sleep_offset_error = timer->sleep_offset_error + time_offset_error;
        }
        else
        {
            // clamp huge deviations to prevent math overflow
            sleep_offset_error = want_delta_time;
        }
        timer->sleep_offset_error = sleep_offset_error;
        timer->previous_wanted_elapsed_time = want_delta_time;
    }
    
    MMTick want_sleep_time = 0;
    if(/*HACK*/(s64)want_delta_time >= sleep_offset_error)
    {
        want_sleep_time = want_delta_time - sleep_offset_error;
    }
    else if(sleep_offset_error > timer->max_offset_error)
    {
        // Clamp lag-based correction to an upper limit
        //  to prevent forwards-skipping.
        // Backwards is not clamped
        //  to avoid upsetting the main timer
        //  if it is low-resolution.
        timer->sleep_offset_error = timer->max_offset_error;
    }
    
    return want_sleep_time;
}

static void mmtimer_reset(struct MMTimer* __restrict timer, MMTick current_time)
{
    timer->previous_current_time = current_time;
    timer->previous_wanted_elapsed_time = 0;
    timer->sleep_offset_error = 0;
}

static void mmtimer_init_default(struct MMTimer* __restrict timer)
{
    timer->max_offset_error = MMIDI_TIMER_DEFAULT_ERROR;
    mmtimer_reset(timer, 0);
}

__declspec(noinline) u8* varlen_decode_slow(u8* __restrict ptr, u32* __restrict out, u8 data)
{
    u32 result = 0;
    result = (result + (data - 0x80)) << 7;
    
    data = *(ptr++); if(data < 0x80) {goto done;}
    result = (result + (data - 0x80)) << 7;
    data = *(ptr++); if(data < 0x80) {goto done;}
    result = (result + (data - 0x80)) << 7;
    //WTF, varlen is undefined above 28bits, it's invalid to have bit7 set for the 4th byte
    data = *(ptr++); goto done;
    
done:
    result += data;
    *out = result;
    return ptr;
}

__forceinline u8* varlen_decode(u8* __restrict ptr, u32* __restrict out)
{
    u8 data = *(ptr++);
    if(__builtin_expect(data < 0x80, 1))
    {
        *out = data;
        return ptr;
    }
    
    return varlen_decode_slow(ptr, out, data);
}

__declspec(noinline) void tracks_merge(MMTrack* __restrict ptr)
{
    for(;;)
    {
        *ptr = *(ptr + 1);
        if(!ptr->ptrs)
            return;
        ++ptr;
    }
}


DWORD WINAPI PlayerThread(PVOID lpParameter)
{
    puts("Hello from player");
    
    MMPlayer* __restrict player = (MMPlayer*)lpParameter;
    
    player->done = false;
    
    extern int WINAPI _VDSO_QueryInterruptTime(PULONGLONG _outtime);
    int(WINAPI*NtDelayExecution)(int doalert, INT64* timeptr) = 0;
    int(WINAPI*NtQuerySystemTime)(PULONGLONG timeptr) = 0;
    
    NtDelayExecution = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtDelayExecution");
    //NtQuerySystemTime = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtQuerySystemTime");
    NtQuerySystemTime = (void*)_VDSO_QueryInterruptTime;
    
    MMTick counter = 0;
#ifndef MODE_BH
    //MMTick bigcounter = ~0;
#endif
    u32 tempomaxsleep = player->SleepTicksMax;
    u32 minsleep = -1;
    
    struct MMTimer* timer = &player->timer;
    mmtimer_init_default(timer);
    
    MMTrack* trk = player->tracks;
    
    for(;;)
    {
        if(!trk->ptrs)
            break;
        
        u8* ptrs = trk->ptrs;
        u8* ptre = trk->ptre;
        
        u32 slep;
        ptrs = varlen_decode(ptrs, &slep);
        
        if(ptrs < ptre)
        {
            trk->ptrs = ptrs;
            trk->nextcounter = slep;
            trk++;
            continue;
        }
        else
        {
            tracks_merge(trk);
            continue;
        }
    }
    
#ifdef MODE_BH
    uint32_t trk_cnt = player->TrackCount;
    bheap* trk_lst = malloc(bheap_alloc_size(trk_cnt));
    trk_lst->count = 0;
    trk_lst->size = trk_cnt;
    MMTrack* *trk_ret = malloc(trk_cnt * sizeof(MMTrack*));
    uint32_t trk_ret_cnt = 0;
    
    {
        MMTrack* __restrict trk2 = player->tracks;
        uint32_t e = bheap_add_begin(trk_lst);
        while(trk2 < trk)
        {
            if(!trk2->ptrs)
                break;
            
            bheap_add(trk_lst, trk2);
            ++trk2;
        }
        bheap_add_end(trk_lst, e);
    }
#endif
    
    {
        u64 time;
        NtQuerySystemTime(&time);
        mmtimer_reset(timer, time);
    }
    
    cbMMShortMsg KShortMsg = player->KShortMsg;
    
    for(;;)
    {
        KShortMsg = player->KShortMsg;
        
        trk = player->tracks;
        
        for(;;)
        {
#ifndef MODE_BH
            if(!trk->ptrs)
                break;
            
            if(trk->nextcounter <= counter)
                ;
            else
            {
                trk++;
                continue;
            }
#else
            MMTrack** h = bheap_peek(trk_lst);
            if(h)
                trk = *h;
            else
                break;
            
            if(trk->nextcounter <= counter)
                ;
            else
                break;
            
            
            bheap_pop(trk_lst, NULL);
#endif
            
            player->CurrentTrack = trk;
            
            u8* __restrict ptrs = trk->ptrs;
            u8* ptre = trk->ptre;
            
            u32 slep = 0;
            
            MMEvent msg;
            msg.dwEvent = trk->event.cmd;
            
            do
            {
                u8 v = *ptrs;
                if(__builtin_expect(v >= 0x80, 1))
                {
                    ptrs++;
                    msg.cmd = v;
                }
                
                u8 swcmd = msg.cmd;
#ifdef NO_ONEKEY
                if(__builtin_expect(swcmd < 0xA0, 1))
                {
                    if(__builtin_expect(swcmd >= 0x90, 1))
                    {
                        msg.prm1 = *(ptrs++);
                        msg.prm2 = *(ptrs++);
                        
                        if((msg.prm2 != 1) || player->SyncPtr)
                            KShortMsg(msg.dwEvent);
                        
                        goto cmdend;
                    }
                    else
                    {
                        msg.prm1 = *(ptrs++);
                        msg.prm2 = *(ptrs++);
                        
                        KShortMsg(msg.dwEvent);
                        
                        goto cmdend;
                    }
                }
#endif
                
                if(COMPILER_LIKELY(swcmd < 0xC0))
                {
                msg_2b:
                    msg.prm1 = *(ptrs++);
                    msg.prm2 = *(ptrs++);
                    
                    KShortMsg(msg.dwEvent);
                    
                    goto cmdend;
                }
                
                if(swcmd < 0xE0)
                {
                    msg.prm1 = *(ptrs++);
                    msg.prm2 = 0;
                    
                    KShortMsg(msg.dwEvent);
                    
                    goto cmdend;
                }
                
                if(swcmd < 0xF0)
                {
                    goto msg_2b;
                }
                
                if(swcmd == 0xFF) // meta
                {
                    u8 meta = *(ptrs++);
                    u32 metalen;
                    ptrs = varlen_decode(ptrs, &metalen);
                    
                    if(meta < 10)
                    {
                        //TODO print text
                    }
                    if(meta == 0x2F)
                    {
                        goto track_merge;
                    }
                    if(meta == 0x51)
                    {
                        u32 newtick = (ptrs[0] << 16) | (ptrs[1] << 8) | (ptrs[2] << 0);
                        player->tempo = newtick;
                    }
                    
                    if(player->KLongMsg)
                    {
                        msg.prm1 = meta;
                        player->KLongMsg(msg.dwEvent, ptrs, metalen);
                    }
                    
                    ptrs += metalen;
                    goto cmdend;
                }
                
                if(swcmd == 0xF0) // SysEx
                {
                    u32 metalen;
                    ptrs = varlen_decode(ptrs, &metalen);
                    
                    if(player->KLongMsg)
                        player->KLongMsg(msg.dwEvent, ptrs, metalen);
                    /*{
                        *(ptrs - 1) = 0xF0;
                        MIDIHDR hdr;
                        zeromem(&hdr, sizeof(hdr));
                        hdr.lpData = ptrs - 1;
                        hdr.dwBufferLength = metalen + 1;
                        hdr.dwBytesRecorded = metalen + 1;
                        hdr.dwFlags = 2;
                        while(KModMessage(0, 8, 0, (DWORD_PTR)&hdr, 0) == 67);
                    }*/
                    
                    ptrs += metalen;
                    goto cmdend;
                }
                
                printf("Bypass\ncmd=%02X msg.cmd=%02X msg=%08X\n", swcmd, msg.cmd, msg.dwEvent);
                
                __builtin_trap();
                
                cmdend:
                
                slep = *(ptrs++);
                
                if(__builtin_expect(!slep, 1))
                {
                    continue;
                }
                
                if(__builtin_expect(ptrs < ptre, 1))
                {
                    --ptrs;
                    ptrs = varlen_decode(ptrs, &slep);
                    
                    MMTick nextctr = trk->nextcounter + slep;
                    trk->nextcounter = nextctr;
                    
                    if(nextctr <= counter)
                        continue;
                    
                    trk->ptrs = ptrs;
                    trk->event.cmd = msg.cmd;
                    
#ifndef MODE_BH
                    //if(trk->nextcounter < bigcounter)
                    //    bigcounter = trk->nextcounter;
#endif
                    
                    goto track_next;
                }
                else
                {
                    printf("Track#%03u ended abruptly\n", trk->trackid);
#ifdef ENDDEAD
                    DWORD dw = 1 << 12;
                    while(dw--)
                        KShortMsg(((dw & 0xFF) << 8) | (dw >> 8) | 0x80);
#endif
                    
                    goto track_merge;
                }
            }
            while(1);
            
            puts("Reached event processor trap, aborting");
            __builtin_trap();
            
            track_next:
#ifndef MODE_BH
            ++trk;
#else
            trk_ret[trk_ret_cnt++] = trk;
#endif
            continue;
            
            track_merge:
#ifndef MODE_BH
            tracks_merge(trk);
#else
            
#endif
            continue;
        }
        
#ifndef MODE_BH
        if(trk == player->tracks)
            break;
        
        //if(bigcounter != ~0)
        //{
        //    minsleep = bigcounter - counter;
        //    bigcounter = ~0;
        //}
        //else
        {
            // I hate this method so much, it's so slow
            
            trk = player->tracks;
            
            for(;;)
            {
                if(!trk->ptrs) break;
                
                DWORD diff = trk->nextcounter - counter;
                if(diff < minsleep)
                    minsleep = diff;
                
                trk++;
            }
            
            if(trk == player->tracks)
                break;
        }
#else
        
        uint32_t lst_res = bheap_add_begin(trk_lst);
        for(uint32_t i = 0; i < trk_ret_cnt; ++i)
            bheap_add(trk_lst, trk_ret[i]);
        trk_ret_cnt = 0;
        bheap_add_end(trk_lst, lst_res);
        
        {
            MMTrack** e = bheap_peek(trk_lst);
            if(!e)
                break;
            
            trk = *e;
        }
        
        minsleep = trk->nextcounter - counter;
#endif
        
        if(!player->SyncPtr)
        {        
            if(minsleep)
            {
                // Cap maximum sleep time, so
                //  tasks that expect frequent updates
                //  still get called enough times per second
                //  to keep them updated with enough granularity.
                if(tempomaxsleep && minsleep > tempomaxsleep)
                    minsleep = tempomaxsleep;
                
                counter += minsleep;
                player->TickCounter = counter;
                
                u64 sleeptime = (u64)minsleep * player->tempo * 10;
                player->RealTimeUndiv += sleeptime;
                
                sleeptime /= (u64)player->timediv;
                player->RealTime += sleeptime;
                
                if(player->KSyncFunc)
                    player->KSyncFunc(player, minsleep);
                
                u64 should_sleep_time;
                {
                    u64 time;
                    NtQuerySystemTime(&time);
                    should_sleep_time = mmtimer_get(timer, time, sleeptime);
                }
                
                if(should_sleep_time)
                {
                    // Negative sleep time to NtDelayExecution is delta sleep
                    // (as opposed to absolute sleep on a positive value)
                    INT64 realsleeptime = -should_sleep_time;
                    NtDelayExecution(0, &realsleeptime);
                }
            }
            
            minsleep = -1;
            
            continue;
        }
        else
        {
            if(player->RealTime < (*player->SyncPtr + player->SyncOffset))
            {
                if(tempomaxsleep && minsleep > tempomaxsleep)
                    minsleep = tempomaxsleep;
                
                counter += minsleep;
                player->TickCounter = counter;
                
                u64 sleeptime = (u64)minsleep * player->tempo * 10;
                player->RealTimeUndiv += sleeptime;
                
                sleeptime /= player->timediv;
                player->RealTime += sleeptime;
                
                if(player->KSyncFunc)
                    player->KSyncFunc(player, minsleep);
            }
            else
            {
                {
                    u64 time;
                    NtQuerySystemTime(&time);
                    mmtimer_reset(timer, time);
                }
                
                INT64 mo = ~0LL;
                NtDelayExecution(0, &mo);
                
                if(player->KSyncFunc)
                    player->KSyncFunc(player, 0);
            }
            
            minsleep = -1;
            
            continue;
        }
    }
    
    player->done = true;
#ifdef MODE_BH
    free(trk_lst);
    free(trk_ret);
#endif
    puts("Player died :(");
    
    return 0;
}

MMPlayer* mmpDuplicatePlayer(const MMPlayer* other)
{
    MMTrack* base = other->tracks;
    DWORD trackcount = other->TrackCount;
    if(!trackcount)
        return 0;
    
    trackcount++;
    MMTrack* tracks = malloc(trackcount * sizeof(MMTrack));
    if(!tracks)
        return 0;
    
    MMPlayer* ply = malloc(sizeof(MMPlayer));
    if(!ply)
    {
        free(tracks);
        return 0;
    }
    
    memcpy(ply, other, sizeof(MMPlayer));
    ply->tracks = tracks;
    memcpy(tracks, base, trackcount * sizeof(MMTrack));
    
    return ply;
}
