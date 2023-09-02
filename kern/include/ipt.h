#ifndef _IPT_H_
#define _IPT_H_

#include <lib.h>
#include <types.h>

#include <coremap.h>


#define IPT_SIZE get_nRamFrames()

/* Debug printing of page table dimension */
#define PRINT_IPT_DIM 1

/* Mask to obtain the displacement from a virtual address */
#define DISPLACEMENT_MASK 0xFFF

/*Page fault types*/
#define INVALID_MAP 1
#define NOT_MAPPED 2 /* there is not a corresponding frame */

struct ipt_entry{
    pid_t p_pid; /* process pid */
    vaddr_t page_number;
    uint32_t status; /* Present or absent */
    uint32_t protection; /* read-only, write, read-write*/
};

/* Values of status */
#define ABSENT 0
#define PRESENT 1

/* Values of protection */
#define IPT_E_RO 0 /* Read-only */
#define IPT_E_WR 1 /* Write */
#define IPT_E_RW 2 /* Read-write */

void ipt_init(void);
void ipt_page_free(unsigned int);
void ipt_clean_up(void);
void ipt_destroy(void);
paddr_t ipt_fault(uint32_t);
void ipt_map(pid_t, vaddr_t, paddr_t);
paddr_t ipt_translate(pid_t, vaddr_t);
void ipt_swap_push(struct ipt_entry*);
void ipt_swap_pop(struct ipt_entry*);

#endif