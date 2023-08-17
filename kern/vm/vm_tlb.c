#ifndef _VM_TLB_H
#define _VM_TLB_H

#include <tlb.h>

int tlb_get_rr_victim(){
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}

// tlb_get_page --> 
// tlb_miss define? chiama la pt_get_page() e fa l'update
// tlb_hit define?
// tlb_update
// tlb_substitute -->




#endif
