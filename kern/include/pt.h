#ifndef _PT_H_
#define _PT_H_

#define PT_SIZE get_nRamFrames()

#define EMPTY 0
#define INVALID_MAP 1

struct pagetable{
    paddr_t paddr[PT_SIZE];
    /* to do */
};


void pt_init(void);
int pt_map(paddr_t, vaddr_t);
paddr_t pt_translate(vaddr_t);


#endif