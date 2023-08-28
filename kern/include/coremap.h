#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>


#define UNDEF_CONSEC_PAGES 0 /* Frames for boot or user */

#define MAX_NUM_ASS_ADDR 16

struct coremap_entry{
    struct addrspace** associated_addr; /* array of pointers to the address spaces associated to the page */
    unsigned long num_assaddr; /* number of address spaces */
    paddr_t frame_addr; /* physical address of the page */
    unsigned long status; /* mark of the single page*/ /* 1=free; 0=allocated; ... */
    unsigned long consec_pages; /* Used only for RESERVED pages, telling us how many contiguous frame we allocated*/
};

/* Values of status */
#define FREE 0
#define OCCUPIED 1
#define RESERVED 2 /*Protection for frames used by the kernel, i.e: frames where coremap is allocated */

struct coremap{
    struct coremap_entry* entry; /* entry of the coremap */
    unsigned long size; /* coremap size */

    struct coremap_entry** np; /* Array of free pages */
    unsigned long np_capacity;
    unsigned long np_tail;
    unsigned long np_head; /* Head of null pages */
    unsigned long np_sz; /*Size of the list of free pages*/
};

/* Should be called during boot*/
void coremap_init(void);

int get_nRamFrames(void);
void coremap_cleanup(void);
unsigned long get_np_sz(void);

/* Contiguous allocation and deallocation used by kernel*/
vaddr_t alloc_kpages(unsigned);
void free_kpages(vaddr_t);

// TO DO: Write this function
paddr_t alloc_upage(void);
void free_upage(paddr_t);

#endif /* _COREMAP_H_ */