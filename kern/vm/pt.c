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

struct pt_entry pagetable[PT_SIZE];

void pt_init(){
    int i;
    pt = kmalloc(sizeof(struct pagetable));
    if (pt==NULL){
        return;
    }
    pt->paddr=kmalloc(sizeof(paddr_t)*PT_SIZE);
    if (pt->paddr==NULL){
        kfree(pt);
        return;
    }
    for (i=0; i<PT_SIZE; i++){
        pt->paddr[i]=EMPTY;
    }
}

void pt_destroy(){
    kfree(pt->paddr);
    kfree(pt);
}

/* p is the physical address of the frame (frame number)*/
/* so, some different virtual addresses have the same physical address (frame)*/
/* frame number != page number */
/* I obtain the page number dividing by PAGE_SIZE; I obtain the frame number from the page table */
void pt_map(paddr_t p, vaddr_t v){
    
    /* PAGE NUMBER */
    unsigned int i = (unsigned int) v/PAGE_SIZE;
    if(i>PT_SIZE){
        return;
    }

    spinlock_acquire(&free_pt);
    pt->paddr[i]=p;
    spinlock_release(&free_pt);
    
    return 0;
}
/* Change this name of the function maybe in pt_get_paddr */
/* Not sure it is the correct way to obtain the physical address */
paddr_t pt_get_page(vaddr_t v){
    /*
    paddr_t res;

    res=v-MIPS_KSEG0;

    res &= ~PAGE_SIZE;

    return res;
    */
}

void pt_fault(struct pt_entry* pt_e, uint32_t faulttype){
    switch(faulttype){
        case INVALID_MAP:
        panic("Invalid input address for paging\n");
        case NOT_MAPPED:

        break;
        default:
        break;
    }
}

paddr_t pt_translate(vaddr_t v){
    paddr_t p; /* physical address of the frame (frame number) */
    /* PAGE NUMBER */
    unsigned int i = (unsigned int) v/PAGE_SIZE;
    if (i>PT_SIZE){
        pt_fault(NULL, INVALID_MAP);
    }
    
    spinlock_acquire(&free_pt);
    /* frame number */
    p = pt->paddr[i];
    if(p==EMPTY){
        /* There's not a corresponding frame */
        /* return ?*/
        /* here, we choose to map*/
        // pt_fault();
        p=pt_get_page(v);
        pt_map(p, v);
    }
    /* physical address to return */
    p |= (v & DISPLACEMENT_MASK);
    spinlock_release(&free_pt);

    return p;
}

