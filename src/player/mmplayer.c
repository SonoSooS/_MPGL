#include <windows.h>
#include <stdio.h>

#include "config.h"
#include "mmplayer.h"

#ifdef MODE_BH
#include "bh.h"
#endif


DWORD WINAPI PlayerThread(PVOID lpParameter)
{
    puts("Hello from player");
    
    MMPlayer* player = (MMPlayer*)lpParameter;
    
    player->done = false;
    
    extern int _VDSO_QueryInterruptTime(PULONGLONG _outtime);
    int(WINAPI*NtDelayExecution)(int doalert, INT64* timeptr) = 0;
    int(WINAPI*NtQuerySystemTime)(ULONGLONG* timeptr) = 0;
    
    NtDelayExecution = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtDelayExecution");
    //NtQuerySystemTime = (void*)GetProcAddress(GetModuleHandle("ntdll"), "NtQuerySystemTime");
    NtQuerySystemTime = (void*)_VDSO_QueryInterruptTime;
    
    MMTick counter = 0;
#ifndef MODE_BH
    //MMTick bigcounter = ~0;
#endif
    DWORD tempomulti = player->tempomulti;
    DWORD tempomaxsleep = player->SleepTicks;
    DWORD minsleep = -1;
    
    if(player->SleepTimeMax)
    {
        if(player->SleepTimeMax > player->tempomulti)
            player->SleepTicks = player->SleepTimeMax / player->tempomulti;
        else
            player->SleepTicks = 1;
    }
    
    ULONGLONG ticker = 0;
    ULONGLONG tickdiff = 0;
    INT32 tdiff = 0;
    INT32 oldsleep = 0;
    INT32 deltasleep = 0;
    
    MMTrack* trk = player->tracks;
    
    for(;;)
    {
        if(!trk->ptrs) break;
        
        BYTE* ptrs = trk->ptrs;
        BYTE* ptre = trk->ptre;
        DWORD slep = 0;
        do
        {
            INT8 v = *(INT8*)(ptrs++);
            slep = (slep << 7) | ((BYTE)v & 0x7F);
            if(v >= 0)
                break;
        }
        while(ptrs < ptre);
        
        if(ptrs < ptre)
        {
            trk->ptrs = ptrs;
            trk->nextcounter = slep;
            trk++;
            continue;
        }
        else
        {
            MMTrack* bkptr = trk;
            for(;;)
            {
                memcpy(bkptr, bkptr + 1, sizeof(*bkptr));
                if(!bkptr->ptrs) break;
                bkptr++;
            }
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
            if(!trk->ptrs) break;
            
            if(trk->nextcounter > counter)
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
            
            BYTE* ptrs = trk->ptrs;
            BYTE* ptre = trk->ptre;
            DWORD slep = 0;
            
            MMEvent msg;
            msg.dwEvent = trk->event.cmd;
            
            do
            {
                INT8 v = *(INT8*)ptrs;
                if(v < 0)
                {
                    msg.cmd = (BYTE)v;
                    ptrs++;
                }
                
                BYTE swcmd = (msg.cmd >> 4) & 7;
                
                if(swcmd == 1)
                {
                    msg.prm1 = *(ptrs++);
                    msg.prm2 = *(ptrs++);
                    
                    if(msg.prm2 != 1 || player->SyncPtr)
                        KShortMsg(*(DWORD*)&msg);
                    
                    goto cmdend;
                }
                
                if(swcmd == 0)
                {
                    msg.prm1 = *(ptrs++);
                    msg.prm2 = *(ptrs++);
                    
                    KShortMsg(*(DWORD*)&msg);
                    
                    goto cmdend;
                }
                
                if(swcmd < 4)
                {
                    msg.prm1 = *(ptrs++);
                    msg.prm2 = *(ptrs++);
                    
                    KShortMsg(*(DWORD*)&msg);
                    
                    goto cmdend;
                }
                
                if(swcmd < 6)
                {
                    msg.prm1 = *(ptrs++);
                    msg.prm2 = 0;
                    
                    KShortMsg(*(DWORD*)&msg);
                    
                    goto cmdend;
                }
                
                if(swcmd == 6)
                {
                    msg.prm1 = *(ptrs++);
                    msg.prm2 = *(ptrs++);
                    
                    KShortMsg(*(DWORD*)&msg);
                    
                    goto cmdend;
                }
                
                if(swcmd == 7)
                {
                    if(msg.cmd == 0xFF)
                    {
                        BYTE meta = *(ptrs++);
                        
                        DWORD metalen = 0;
                        do
                        {
                            INT8 v = *(INT8*)(ptrs++);
                            metalen = (metalen << 7) | ((BYTE)v & 0x7F);
                            if(v >= 0)
                                break;
                        }
                        while(ptrs < ptre);
                        
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
                            DWORD newtick = (ptrs[0] << 16) | (ptrs[1] << 8) | (ptrs[2] << 0);
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
                            player->KLongMsg(*(DWORD*)&msg, ptrs, metalen);
                        }
                        
                        ptrs += metalen;
                        goto cmdend;
                    }
                    if(msg.cmd == 0xF0)
                    {
                        DWORD metalen = 0;
                        do
                        {
                            INT8 v = *(INT8*)(ptrs++);
                            metalen = (metalen << 7) | ((BYTE)v & 0x7F);
                            if(v >= 0)
                                break;
                        }
                        while(1);
                        
                        if(player->KLongMsg)
                            player->KLongMsg(*(DWORD*)&msg, ptrs, metalen);
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
                }
                
                printf("Bypass\ncmd=%02X msg.cmd=%02X msg=%08X\n", swcmd, msg.cmd, *(DWORD*)&msg);
                
                __builtin_trap();
                
                cmdend:
                
                //printf("command %08X\n", *(DWORD*)&msg);
                
                slep = 0;
                do
                {
                    INT8 v = *(INT8*)(ptrs++);
                    slep = (slep << 7) | ((BYTE)v & 0x7F);
                    if(v >= 0)
                        break;
                }
                while(ptrs < ptre);
        
                if(ptrs < ptre)
                {
                    trk->nextcounter += slep;
                    if(trk->nextcounter > counter)
                    {
                        trk->ptrs = ptrs;
                        trk->event.cmd = msg.cmd;
                        
#ifndef MODE_BH
                        //if(trk->nextcounter < bigcounter)
                        //    bigcounter = trk->nextcounter;
#endif
                        
                        goto track_next;
                    }
                    continue;
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
            {
                MMTrack* bkptr = trk;
                for(;;)
                {
                    memcpy(bkptr, bkptr + 1, sizeof(*bkptr));
                    if(!bkptr->ptrs) break;
                    bkptr++;
                }
            }
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
