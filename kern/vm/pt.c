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

static struct pagetable* pt;

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

//Chi la chiama? "p" è l'indirizzo fisico specifico o della pagina?
void pt_map(paddr_t p, vaddr_t v){
    unsigned int i = (unsigned int) v/PAGE_SIZE;
    if(i>PT_SIZE){
        return;
    }

    spinlock_acquire(&free_pt);
    pt->paddr[i]=p;
    spinlock_release(&free_pt);

    return 0;
}

paddr_t pt_get_page(vaddr_t v){
    paddr_t res;
    res=v-MIPS_KSEG0;

    /* Attenzione: fare check su res perché potrebbe non essere mai stato caricato in memoria */

    /* Align */
    res &= ~PAGE_SIZE;

    v=res;
    return v;
}

paddr_t pt_translate(vaddr_t v){
    paddr_t p;
    unsigned int i = (unsigned int) v/PAGE_SIZE;
    if (i>PT_SIZE){
        return INVALID_MAP;
    }

    
    spinlock_acquire(&free_pt);
    p = pt->paddr[i];
    if(p==EMPTY){
        /* There's not a corresponding frame */
        p=pt_get_page(v);
        pt_map(res, v);
    }
    spinlock_release(&free_pt);

    return p;
}

