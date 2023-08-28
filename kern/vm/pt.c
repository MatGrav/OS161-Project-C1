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

#include <types.h>
#include <lib.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>
#include <swapfile.h>

#include "pt.h"

struct spinlock free_pt = SPINLOCK_INITIALIZER;

/* Page Table */
// TO DO: check 
struct pt_entry* pt = NULL;

/* **Queue for FIFO support** */
/* queue_fifo contains page numbers (index)*/
//static unsigned int queue_fifo[PT_SIZE];
unsigned int* queue_fifo = NULL;
static unsigned int queue_front = 0;
static unsigned int queue_rear = 0;

void pt_init(){
    int i;
    pt = (struct pt_entry*) kmalloc(sizeof(struct pt_entry)*PT_SIZE);
    if (pt==NULL){
        return;
    }
    pt_clean_up();

    queue_fifo = kmalloc(sizeof(unsigned int)*PT_SIZE);

    for (i=0; i<PT_SIZE; i++){
        queue_fifo[i]=0;
    }
}

/* Pop on queue_fifo */
static unsigned int pt_fifo() {
    KASSERT(queue_front != queue_rear);

    /* index of old page to pop */
    unsigned int old = queue_fifo[queue_front];
    //write on swapfile -> call a function in swapfile.c
    pt_swap_push(&pt[old]);
    pt_page_free(old);
    queue_front = (queue_front + 1) % PT_SIZE;
    
    return old;
}

/* L'ho scritto in una funzione a parte perché potrebbe tornarci utile per azzerare la pt*/
void pt_clean_up(){
    int i;
    for (i=0; i<PT_SIZE; i++){
        pt[i].paddr=0;
        pt[i].status=ABSENT;
        pt[i].protection=PT_E_RW;
    }
}

void pt_page_free(unsigned int i){
    pt[i].paddr=0;
    pt[i].status=ABSENT;
    pt[i].protection=PT_E_RW;
}

void pt_destroy(){
    kfree(pt);
}

/* p is the physical address of the frame (frame number)*/
/* so, some different virtual addresses have the same physical address (frame)*/
/* frame number != page number */
/* I obtain the page number dividing by PAGE_SIZE; I obtain the frame number from the page table */
void pt_map(paddr_t p, vaddr_t v){
    
    KASSERT(p!=0);
    KASSERT(v!=0);

    /* To be sure it is aligned */
    p &= PAGE_FRAME;


    /* PAGE NUMBER */
    int i = (int) v/PAGE_SIZE;
    if(i>PT_SIZE){
        return;
    }

    spinlock_acquire(&free_pt);
    pt[i].paddr=p;
    if(pt[i].status==ABSENT){
        pt[i].status=PRESENT;
    }
    pt[i].protection=PT_E_RW;
    
    /* Push on queue_fifo */
    queue_fifo[queue_rear] = i; /* we write the index of page table */
    queue_rear = (queue_rear + 1) % PT_SIZE;  /* update of rear */
    spinlock_release(&free_pt);
}

void pt_fault(struct pt_entry* pt_e, uint32_t faulttype){
    unsigned int i;
    switch(faulttype){
        case INVALID_MAP:
        (void)pt_e;
        panic("Invalid input address for paging\n");
        case NOT_MAPPED:
        {
        /* Wr're trying to access to a not mapped page */
        /* Let's update it in memory and so in page table */

        paddr_t p = alloc_upage();

        if(p==0){
            /* there's not enough space -> substitute */

            
            i = pt_fifo(); //Liberazione nella pt
            free_upage(pt[i].paddr); //Liberazione nella "coremap"
            p = alloc_upage();         
            //pt_map(pt_e->paddr, v);
        } else {
            
        }
        pt_e->paddr = p;


        }
        break;
        default:
        break;
    }
}

paddr_t pt_translate(vaddr_t v){
    paddr_t p; /* physical address of the frame (frame number) */
    /* PAGE NUMBER */
    int i = (int) (v/PAGE_SIZE);
    
    if (i>PT_SIZE){
        pt_fault(NULL, INVALID_MAP);
    }
    
    spinlock_acquire(&free_pt);
    /* frame number */
    p = pt[i].paddr;
    spinlock_release(&free_pt);

    if(pt[i].status == ABSENT){
        /* There's not a corresponding frame */
        pt_fault(&pt[i], NOT_MAPPED);
        /* Dopo questa istruzione, in pt[i] c'è l'indirizzo fisico cui posso scrivere*/
        pt_map(pt[i].paddr,v);

        spinlock_acquire(&free_pt);
        p=pt[i].paddr;
        spinlock_release(&free_pt);
    }
    /* physical address to return */
    p |= (v & DISPLACEMENT_MASK);

    return p;
}

void pt_swap_push(struct pt_entry* pt_e){
    swap_in(pt_e->paddr);
}

paddr_t pt_swap_pop(struct pt_entry* pt_e){
    paddr_t res;

    res=swap_out(pt_e->paddr);
    if(res){
        //perror("pt_swap_pop: unable to pop paddr 0x%d from swapfile\n", pt_e->paddr);
    }

    return res;
}