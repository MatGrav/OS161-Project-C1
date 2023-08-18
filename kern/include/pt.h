#ifndef _PT_H_
#define _PT_H_

#include <lib.h>
#include <types.h>

#include <coremap.h>

// TO DO : sOlve this problem
//The following thing does not compile
//#define PT_SIZE get_nRamFrames()/2

#define PT_SIZE 1024*1024

#define EMPTY 0
#define INVALID_MAP 1

/* Fare FORSE struct page e poi in pagetable vettore di page */

struct pagetable{
    paddr_t paddr[PT_SIZE];
    /* to do */
};


void pt_init(void);
void pt_destroy(void);
paddr_t pt_get_page(vaddr_t);
void pt_map(paddr_t, vaddr_t);
paddr_t pt_translate(vaddr_t);


#endif