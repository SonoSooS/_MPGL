#include <stdbool.h>

// return true if left goes lower than right
static bool track_cmp(const MMTrack* __restrict left, const MMTrack* __restrict right)
{
    if(left->nextcounter < right->nextcounter)
        return false;
    
    if(left->nextcounter == right->nextcounter)
        return left->trackid >= right->trackid;
    
    return true;
}

#define HEAP_NO_CHECKS
#define HEAP_FNPRIV static
#define HEAP_FNPUB static
#define HEAP_PREFIX b
#define HEAP_TYPE MMTrack*
// return left >= right
#define HEAP_CMP(x, y) track_cmp(x, y)
#include "C:\\Data\\lolol\\bheap\\heap.h"
