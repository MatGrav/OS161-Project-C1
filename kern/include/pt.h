#ifndef _PT_H_
#define _PT_H_

#include <lib.h>
#include <types.h>

#include <coremap.h>

// TO DO : sOlve this problem
//The following thing does not compile
//#define PT_SIZE get_nRamFrames()/2

#define PT_SIZE 1024*1024

/* Mask to obtain the displacement from a virtual address */
#define DISPLACEMENT_MASK 0xFFF

/*Page fault types*/
#define INVALID_MAP 1
#define NOT_MAPPED 2 /* there is not a corresponding frame */

struct pt_entry{
    paddr_t paddr; /* physical address */
    uint32_t status; /* Present or absent */
    uint32_t protection; /* read-only, write, read-write*/
    /*
    uint32_t modified;
    uint32_t referenced;
    uint32_t caching_disabled;
    */
};

/* Values of status */
#define ABSENT 0
#define PRESENT 1

/* Values of protection */
#define PT_E_RO 0 /* Read-only */
#define PT_E_WR 1 /* Write */
#define PT_E_RW 2 /* Read-write */

void pt_init(void);
void pt_free(unsigned int);
void pt_clean_up(void);
void pt_destroy(void);
void pt_fault(struct pt_entry*, uint32_t);
void pt_map(paddr_t, vaddr_t);
paddr_t pt_translate(vaddr_t);
void pt_swap_push(struct pt_entry*);
struct pt_entry pt_swap_pop();

#endif