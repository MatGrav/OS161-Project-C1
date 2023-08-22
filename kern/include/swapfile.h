#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#define SWAPFILE_SIZE 9*1024*1024

struct swapfile_entry{
    paddr_t paddr;
    size_t index;
};

void swap_init(void);
void swap_clean_up(void);
void swap_in(paddr_t);
paddr_t swap_out(paddr_t);


#endif