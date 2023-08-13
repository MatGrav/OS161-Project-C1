/*#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>*/

#include "pt.h"
#include <lib.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>

struct spinlock free_pt = SPINLOCK_INITIALIZER;

static struct pagetable pt;

void pt_init(){
    int i;
    for (i=0; i<PT_SIZE; i++){
        pt->paddr[i]=EMPTY;
    }
}

int pt_map(paddr_t p, vaddr_t v){
    unsigned int i = (unsigned int) v/PAGE_SIZE;
    if(i>PT_SIZE){
        return INVALID_MAP;
    }

    spinlock_acquire(&free_pt);
    pt->paddr[i]=p;
    spinlock_release(&free_pt);

    return 0;
}


paddr_t pt_translate(vaddr_t v){
    paddr_t p;
    unsigned int i = (unsigned int) v/PAGE_SIZE;
    if (i>PT_SIZE){
        return INVALID_MAP;
    }

    spinlock_acquire(&free_pt);
    p = pt->paddr[i];
    spinlock_release(&free_pt);

    return p;
}

