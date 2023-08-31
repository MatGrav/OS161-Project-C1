#ifndef _PT_H_
#define _PT_H_

#include <lib.h>
#include <types.h>

#include <coremap.h>

#define PT_SIZE get_nRamFrames()

/* Mask to obtain the displacement from a virtual address */
#define DISPLACEMENT_MASK 0xFFF

/*Page fault types*/
#define INVALID_MAP 1
#define NOT_MAPPED 2 /* there is not a corresponding frame */

#define PRINT_IPT_DIM 1


struct pt_entry {
    vaddr_t vaddr; /* virtual address*/
    uint8_t status; /* Present or absent */
    uint8_t protection; /* read-only, write, read-write*/
};


/* Values of status */
#define ABSENT 0
#define PRESENT 1

/* Values of protection */
#define PT_E_RO 0 /* Read-only */
#define PT_E_WR 1 /* Write */
#define PT_E_RW 2 /* Read-write */

void pt_init(void);
void pt_page_free(unsigned int);
void pt_clean_up(void);
void pt_destroy(void);
paddr_t pt_fault(uint32_t);
void pt_map(paddr_t, vaddr_t);
paddr_t pt_translate(vaddr_t);
void pt_swap_push(struct pt_entry*);
paddr_t pt_swap_pop(struct pt_entry*);

#endif