#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>

#define FREE 0
#define OCCUPIED 1
#define RESERVED 2 /*Protection for pages used by the kernel, i.e: frames where coremap is allocated */

#define MAX_NUM_ASS_ADDR 16

struct coremap_entry{
    struct addrspace** associated_addr; /* array of pointers to the address spaces associated to the page */
    unsigned long num_assaddr; /* number of address spaces */
    paddr_t frame_addr; /* physical address of the page */
    unsigned long status; /* mark of the single page*/ /* 1=free; 0=allocated; ... */
};

struct coremap{
    struct coremap_entry* entry; /* entry of the coremap */
    unsigned long size; /* coremap size */

    struct coremap_entry** np; /* Array of free pages */
    unsigned long np_capacity;
    unsigned long np_tail;
    unsigned long np_head; /* Head of null pages */
    unsigned long np_sz; /*Size of the list of free pages*/
};

void coremap_init();

void coremap_cleanup();

static int isCoremapActive();

//static paddr_t getoneppage(struct addrspace* ?); ?
//static paddr_t* getfreeppages(unsigned long); ?
int getfreeppages(unsigned long, paddr_t addr[]);

static paddr_t getppages(unsigned long);

static int freeppages(paddr_t, unsigned long);

vaddr_t alloc_kpages(unsigned);

void free_kpages(vaddr_t);


void rem_head();

#endif /* _COREMAP_H_ */