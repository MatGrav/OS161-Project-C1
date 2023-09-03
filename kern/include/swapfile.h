#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#define SWAPFILE_SIZE 9*1024*1024

/* Values for bitmap */
#define SF_ABSENT 0
#define SF_PRESENT 1

void swap_init(void);
void swap_clean_up(void);
void swap_in(paddr_t); /* Read from swapfile */
void swap_out(paddr_t); /* Writes to swapfile */


#endif