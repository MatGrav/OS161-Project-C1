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
#include <types.h>

struct spinlock free_pt = SPINLOCK_INITIALIZER;

/* Page Table */
struct pt_entry pt[PT_SIZE];

/* Queue for FIFO support */
static unsigned int queue_fifo[PT_SIZE];
static unsigned int queue_front = 0;
static unsigned int queue_rear = 0;

static unsigned int pt_fifo() {
    KASSERT(queue_front != queue_rear);

    /* Pop on queue_fifo */
    unsigned int old = queue_fifo[queue_front];
    // TO DO: write on swapfile -> call a function in swapfile.c
    pt_free(queue_front);
    queue_front = (queue_front + 1) % PT_SIZE;
    
    return old;
}


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

void pt_free(unsigned int i){
    pt[i]->paddr=0;
    pt[i]->status=ABSENT;
    pt[i]->protection=PT_E_RW;
}

void pt_destroy(){
    kfree(pt);
}

/* p is the physical address of the frame (frame number)*/
/* so, some different virtual addresses have the same physical address (frame)*/
/* frame number != page number */
/* I obtain the page number dividing by PAGE_SIZE; I obtain the frame number from the page table */
void pt_map(paddr_t p, vaddr_t v){
    
    //TO DO allineamento p + KASSERT

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
    
    /* Push on queue_fifo */
    queue[queue_rear] = i; /* we write the index of page table */
    queue_rear = (queue_rear + 1) % PT_SIZE;  /* update of rear */
    spinlock_release(&free_pt);
}

void pt_fault(struct pt_entry* pt_e, uint32_t faulttype){
    unsigned int i;
    switch(faulttype){
        case INVALID_MAP:
        panic("Invalid input address for paging\n");
        case NOT_MAPPED:
        /* Wr're trying to access to a not mapped page */
        /* Let's update it in memory*/
        paddr_t p = getppages(1);

        if(p==0){
            i = pt_fifo();
            pt_map(pt[i]->paddr, PAGE_SIZE*i /* =vaddr*/);
        }
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
    spinlock_release(&free_pt);

    if(pt[i]->status == ABSENT){
        /* There's not a corresponding frame */
        pt_fault(pt[i], NOT_MAPPED);
        spinlock_acquire(&free_pt);
        p=pt[i]->paddr;
        spinlock_release(&free_pt);
    }
    /* physical address to return */
    p |= (v & DISPLACEMENT_MASK);

    return p;
}

void pt_swap_push(struct pt_entry* pt_e){
    swap_in(pt_e->paddr);
}

struct pt_entry pt_swap_pop(struct pt_entry* pt_e){
    struct pt_entry res;
    res=swap_out(pt_e);
}