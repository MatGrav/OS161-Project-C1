#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>

struct coremap_entry{
    struct addrspace** associated_addr; /* array of pointers to the address spaces associated to the page */
    unsigned long num_assaddr; /* number of address spaces */
    paddr_t frame_addr; /* physical address of the page */
    unsigned long is_free; /* mark of the single page*/ /* 1=free; 0=allocated; ... */
};

struct coremap{
    struct coremap_entry* entry; /* entry of the coremap */
    unsigned long size; /* coremap size */
};



#endif