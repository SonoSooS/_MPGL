#include <windows.h>
#include <stdio.h>

#include "config.h"
#include "mmplayer.h"

#ifdef MODE_BH
#include "bh.h"
#endif

__declspec(noinline) u8* varlen_decode_slow(u8* ptr, u32* __restrict out, u8 data)
{
    u32 result = (data & 0x7F) << 7;
    
    data = *(ptr++); if(data < 0x80) {goto done;}
    result = (result + (data & 0x7F)) << 7;
    data = *(ptr++); if(data < 0x80) {goto done;}
    result = (result + (data & 0x7F)) << 7;
    //WTF, varlen is undefined above 28bits, it's invalid to have bit7 set for the 4th byte
    data = *(ptr++); goto done;
    
done:
    result += data;
    *out = result;
    return ptr;
}

__forceinline u8* varlen_decode(u8* ptr, u32* __restrict out)
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
    
    MMPlayer* player = (MMPlayer*)lpParameter;
    
    player->done = false;
    
    extern int WINAPI _VDSO_QueryInterruptTime(PULONGLONG _outtime);
    int(WINAPI*NtDelayExecution)(int doalert, INT64* timeptr) = 0;
    int(WINAPI*NtQuerySystemTime)(ULONGLONG* timeptr) = 0;
    
    NtDelayExecution = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtDelayExecution");
    //NtQuerySystemTime = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtQuerySystemTime");
    NtQuerySystemTime = (void*)_VDSO_QueryInterruptTime;
    
    MMTick counter = 0;
#ifndef MODE_BH
    //MMTick bigcounter = ~0;
#endif
    u32 tempomulti = player->tempomulti;
    u32 tempomaxsleep = player->SleepTicks;
    u32 minsleep = -1;
    
    if(player->SleepTimeMax)
    {
        if(player->SleepTimeMax > player->tempomulti)
            player->SleepTicks = player->SleepTimeMax / player->tempomulti;
        else
            player->SleepTicks = 1;
    }
    
    MMTick ticker = 0;
    MMTick tickdiff = 0;
    s32 tdiff = 0;
    s32 oldsleep = 0;
    s32 deltasleep = 0;
    
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
    bh* trk_lst = malloc(bh_c(trk_cnt));
    bh_ct(trk_lst, trk_cnt);
    MMTrack* *trk_ret = malloc(trk_cnt * sizeof(MMTrack*));
    uint32_t trk_ret_cnt = 0;
    
    {
        MMTrack* __restrict trk2 = player->tracks;
        uint32_t e = bh_s(trk_lst);
        while(trk2 < trk)
        {
            if(!trk2->ptrs)
                break;
            
            bh_a(trk_lst, trk2);
            ++trk2;
        }
        bh_q(trk_lst, e);
    }
#endif
    
    NtQuerySystemTime(&tickdiff);
    
    int(WINAPI*KShortMsg)(DWORD msg) = player->KShortMsg;
    
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
            MMTrack** h = bh_n(trk_lst);
            if(h)
                trk = *h;
            else
                break;
            
            if(trk->nextcounter <= counter)
                ;
            else
                break;
            
            bh_d(trk_lst, NULL);
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
                
                if(swcmd < 0xC0)
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
                
                if(msg.cmd == 0xFF) // meta
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
                        player->tempomulti = (player->tempo * 10) / player->timediv;
                        if(player->SleepTimeMax)
                        {
                            if(player->SleepTimeMax > player->tempomulti)
                                player->SleepTicks = player->SleepTimeMax / player->tempomulti;
                            else
                                player->SleepTicks = 1;
                        }
                        tempomaxsleep = player->SleepTicks;
                        tempomulti = player->tempomulti;
                    }
                    
                    if(player->KLongMsg)
                    {
                        msg.prm1 = meta;
                        player->KLongMsg(msg.dwEvent, ptrs, metalen);
                    }
                    
                    ptrs += metalen;
                    goto cmdend;
                }
                if(msg.cmd == 0xF0) // SysEx
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
        
        uint32_t lst_res = bh_s(trk_lst);
        for(uint32_t i = 0; i < trk_ret_cnt; ++i)
            bh_a(trk_lst, trk_ret[i]);
        trk_ret_cnt = 0;
        bh_q(trk_lst, lst_res);
        
        {
            MMTrack** e = bh_n(trk_lst);
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
                if(tempomaxsleep && minsleep > tempomaxsleep)
                    minsleep = tempomaxsleep;
                counter += minsleep;
                INT32 sleeptime = (INT32)(minsleep * tempomulti);
                player->RealTimeUndiv += minsleep * player->tempo * 10;
                player->RealTime += sleeptime;
                player->TickCounter = counter;
                
                NtQuerySystemTime(&ticker);
                tdiff = (INT32)(ticker - tickdiff);
                tickdiff = ticker;
                
                INT32 delt = (INT32)(tdiff - oldsleep);
                oldsleep = sleeptime;
                
                deltasleep += delt;
                
                if(deltasleep > 0)
                    sleeptime -= deltasleep;
                
                if(player->KSyncFunc)
                    player->KSyncFunc(player, minsleep);
                
            #ifdef DEBUGTEXT
                //printf("\rtimer %20lli %10i %10i    ", ticker, sleeptime, deltasleep);
                player->_debug_deltasleep = deltasleep;
                player->_debug_sleeptime = sleeptime;
            #endif
                
                //improves crash performance
                if(sleeptime <= 0)
                {
                    if(deltasleep > 100000)
                        deltasleep = 100000;
                }
                else
                {
                    //printf("Sleep %016llX\n", sleeptime);
                    INT64 realsleeptime = -sleeptime;
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
                DWORD sleeptime = (DWORD)(minsleep * tempomulti);
                player->RealTimeUndiv += minsleep * player->tempo * 10;
                player->RealTime += sleeptime;
                
                if(player->KSyncFunc)
                    player->KSyncFunc(player, minsleep);
            }
            else
            {
                NtQuerySystemTime(&ticker);
                tickdiff = ticker;
                
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
