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

/* Page Table */
struct pt_entry pt[PT_SIZE];

void pt_init(){
    int i;
    pt = kmalloc(sizeof(struct pt_entry)*PT_SIZE);
    if (pt==NULL){
        return;
    }
    
    pt_clean_up();
}

/* L'ho scritto in una funzione a parte perché potrebbe tornarci utile per azzerare la pt*/
void pt_clean_up(){
    for (i=0; i<PT_SIZE; i++){
        pt[i]->paddr=0;
        pt[i]->status=ABSENT;
        pt[i]->protection=PT_E_RW;
    }
}

void pt_destroy(){
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
    pt[i]->paddr=p;
    if(pt[i]->status==ABSENT){
        pt[i]->status=PRESENT;
    }
    pt[i]->protection=PT_E_RW;
    spinlock_release(&free_pt);
}
/* Change this name of the function maybe in pt_get_paddr */
/* Not sure it is the correct way to obtain the physical address */
/* Secondo me è totalmente errata percjé sennò non avrebbe senso
di esistere una page table, basterebbe fare una sottrazione.
Poi non considera che un indirizzo virtuale (pagina) può essere in
comune a più indirizzi fisici (frame) */
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
        // pt_miss(pt_e)?
        // potrebbe essere una funzione che gestisce il caso in cui un frame non è mappato sulla tabella delle pagine
        // cosa può fare? mappare l'indirizzo e restituirlo?
        
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
    p = pt[i]->paddr;
    if(pt[i]->status == ABSENT){
        /* There's not a corresponding frame */
        /* return ?*/
        /* here, we choose to map*/
        pt_fault(pt[i], NOT_MAPPED);
        //p=pt_get_page(v);
        //pt_map(p, v);
    }
    /* physical address to return */
    p |= (v & DISPLACEMENT_MASK);
    spinlock_release(&free_pt);

    return p;
}

