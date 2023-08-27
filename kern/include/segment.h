#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <types.h>

struct segment{
    vaddr_t vaddr; /* Virtual address of the segment */
    size_t memsize; /* Segment size reserved for the segment in virtual memory */
    size_t npage; /* Number of pages, useful for paging algorithm */
    size_t filesize; /* Segment size in the ELF file */
    size_t offset;
    unsigned int is_loaded;
    unsigned int permission;
    struct vnode* file_elf; // serve per la load_segment
    struct addrspace* as; //puntatore all'address space a cui appartiene, serve alla load_segment
    /* Maybe we need to add a spinlock? to work on it */
};

/* Values of is_loaded */
#define NOT_LOADED 0
#define PARTIALLY_LOADED 1
#define TOTALLY_LOADED 2

/* Values of permission */
#define S_RO 0 /* Read Only */
#define S_EX 1 /* Execute */
#define S_RW 2 /* Read-write*/

struct segment* segment_create(void);
void segment_init(struct segment*);
void segment_destroy(struct segment*);
int segment_copy(struct segment*, struct segment**);
int segment_prepare_load(struct segment*);

#endif