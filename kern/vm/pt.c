#include <types.h>
#include <lib.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>
#include <swapfile.h>

#include <novavm.h>
#include <machine/vm.h>

#include "pt.h"


#define SOMEMASK 0x00A00000
#define SECOND_DIM 32


struct spinlock free_pt = SPINLOCK_INITIALIZER;

/* Page Table */
struct pt_entry* pt = NULL;

/* **Queue for FIFO support** */
/* queue_fifo contains page numbers (index)*/
//static unsigned int queue_fifo[PT_SIZE];
unsigned int* queue_fifo = NULL;
static unsigned int queue_front = 0;
static unsigned int queue_rear = 0;

uint16_t (*buf) [SECOND_DIM+1];


/* Returns index where virtual address is, found is to be checked after calling */
static unsigned int pt_search(vaddr_t v, bool* found){
    unsigned int i,k;
    
    *found = false;
    spinlock_acquire(&free_pt);


    /* TO DO: Add Hashing*/

    /* TEST */
    v &= PAGE_FRAME;

    int buf_index = v/SOMEMASK;
    for(k=0;k<SECOND_DIM;k++){
        i = buf[buf_index][k];
        if(pt[i].vaddr == v){
            *found = true;
            spinlock_release(&free_pt);
            return i;
            break;
        }
    }


    /* Se ci sono collisioni o altro */
    

    for(i=0;i<PT_SIZE;i++){
        if(pt[i].vaddr == v){
            *found = true;
            break;
        }
    }

    spinlock_release(&free_pt);
    return i;
}


void pt_init(){
    pt = (struct pt_entry*) kmalloc(sizeof(struct pt_entry)*PT_SIZE);
    if (pt==NULL){
        return;
    }
    pt_clean_up();

    /*for (uint8_t i = 0; i < (USERSTACK/SOMEMASK); i++) {
        buf[i] = NULL;
    }*/
    buf = kmalloc((USERSTACK/SOMEMASK)*sizeof(uint16_t[1+SECOND_DIM]));
    if (buf==NULL){  return;  }
    for (unsigned long i=0; i<(USERSTACK/SOMEMASK); i++){
        for(uint8_t j = 0; j<SECOND_DIM; j++){
            buf[i][j] = 0;
        }
    }

    queue_fifo = kmalloc(sizeof(uint8_t)*PT_SIZE);
    if (queue_fifo==NULL){  return;  }

#if PRINT_IPT_DIM
    kprintf("Dimension of a single IPT entry: %d B\n",sizeof(struct pt_entry));
    kprintf("Current PT_SIZE: %d (# of frames/pages)\n",PT_SIZE);
    kprintf("Dimension of page table %d KB \n",(sizeof(struct pt_entry)*PT_SIZE)/1024);
    kprintf("Size of queue fifo %d KB \n",(sizeof(unsigned int)*PT_SIZE)/1024);
    kprintf("Size of buf %d KB\n\n",(sizeof(uint16_t)*(USERSTACK/SOMEMASK)*(1+SECOND_DIM)/1024));
#endif

    for (unsigned long i=0; i<PT_SIZE; i++){
        queue_fifo[i]=0;
    }
}

/* Pop on queue_fifo */
/* Push on queue_fifo is done in pt_map */
static unsigned int pt_queue_fifo_pop() {
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
    unsigned long i;
    for (i=0; i<PT_SIZE; i++){
        pt[i].vaddr = 0;
        //pt[i].paddr=i*PAGE_SIZE;
        pt[i].status=ABSENT;
        pt[i].protection=PT_E_RW;
    }
}

void pt_page_free(unsigned int i){
    pt[i].vaddr = 0;
    //pt[i].paddr=0;
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
    v &= PAGE_FRAME;

    int buf_index = v/SOMEMASK;

    /* Index in IPT based on physical address p */
    int i = (int) p/PAGE_SIZE;

    spinlock_acquire(&free_pt);
    pt[i].vaddr=v;
    //pt[i].pid = something;
    if(pt[i].status==ABSENT){
        pt[i].status=PRESENT;
    }
    pt[i].protection=PT_E_RW;

    uint16_t circular_index = buf[buf_index][0];
    circular_index += 1;
    circular_index = (circular_index % SECOND_DIM) + 1;

    buf[buf_index][circular_index] = i;
    buf[buf_index][0] = circular_index;
    spinlock_release(&free_pt);

    /* TO DO: Should be checked for IPT */
    /* Push on queue_fifo and check on swapfile */
    spinlock_acquire(&free_pt);
    paddr_t res = pt_swap_pop(&pt[i]); /* pop on swapfile if paddr is written there */
    queue_fifo[queue_rear] = i; /* we write the index of page table */
    queue_rear = (queue_rear + 1) % PT_SIZE;  /* update of rear */
    spinlock_release(&free_pt);

    (void)res;
}

paddr_t pt_fault(uint32_t faulttype){
    unsigned int i;
    switch(faulttype){
        case INVALID_MAP:
        panic("Invalid input address for paging\n");
        
        case NOT_MAPPED:
        {
        /* Wr're trying to access to a not mapped page */
        /* Let's update it in memory and so in page table */
        
        paddr_t p = alloc_upage();

        if(p==0){
            /* there's not enough space -> substitute */    
            i = pt_queue_fifo_pop(); //Liberazione nella pt
            free_upage(i*PAGE_SIZE); //Liberazione nella "coremap"
            p=alloc_upage(); /* new address -> later, look if it is written in swapfile*/
        }

        /* Remember: p must be mapped -> look at pt_translate() */
        return p;

        }
        break;
        default:
        break;
    }
    return 0;
}

paddr_t pt_translate(vaddr_t v){
    paddr_t p; /* physical address of the frame (frame number) */
    
    unsigned i;
    

    /* Alignment of virtual address to page */
    v &= PAGE_FRAME;
    if(v >= USERSTACK){
        pt_fault(INVALID_MAP);
    }

    /* Search of the virtual address inside the IPT */
    bool found = false;
    i = pt_search(v,&found);

    if(found){
        p = PAGE_SIZE*i;
    }
    else {
        p = pt_fault(NOT_MAPPED);
        pt_map(p,v);
    }

    return p;
}

void pt_swap_push(struct pt_entry* pt_e){
    (void)pt_e;
    //swap_in(pt_e->paddr);
}

paddr_t pt_swap_pop(struct pt_entry* pt_e){
    paddr_t res = 0;
    (void)pt_e;
    //res=swap_out(pt_e->paddr);

    return res;
}