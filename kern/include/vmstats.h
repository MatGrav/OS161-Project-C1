#ifndef _VMSTATS_H_
#define _VMSTATS_H

#define E_STATS_OVER 1
#define E_STATS_INACTIVE 2


/* • TLB Faults: 
The number of TLB misses that have occurred (not including faults that cause a program to crash). */
#define TLB_FAULTS 0

/*
• TLB Faults with Free: 
The number of TLB misses for which there was free space in the TLB to add the new TLB entry 
(i.e., no replacement is required). */
#define TLB_FAULTS_FREE 1

/*
• TLB Faults with Replace: The number of TLB misses for which there was no free space 
for the new TLB entry, so replacement was required.  */
#define TLB_FAULTS_REPLACE 2

/* • TLB Invalidations: The number of times the TLB was invalidated (this counts the number 
times the entire TLB is invalidated NOT the number of TLB entries invalidated)  */
#define TLB_INVALIDATIONS 3

/* • TLB Reloads: The number of TLB misses for pages that were already in memory. */
#define TLB_RELOADS 4

/* • Page Faults (Zeroed): The number of TLB misses that required a new page to be zerofilled. */
#define PAGE_FAULTS_ZEROED 5

/* • Page Faults (Disk): The number of TLB misses that required a page to be loaded from disk. */
#define PAGE_FAULTS_DISK 6

/* • Page Faults from ELF: The number of page faults that require getting a page from the ELF file. */
#define PAGE_FAULTS_ELF 7

/* • Page Faults from Swapfile: The number of page faults that require getting a page from the swap file. */
#define PAGE_FAULTS_SWAPFILE 8

/* • Swapfile Writes: The number of page faults that require writing a page to the swap file. */
#define SWAPFILE_WRITES 9

/* The sum of “TLB Faults with Free” and “TLB Faults with Replace” should be equal to “TLB Faults.
The sum of “TLB Reloads,” “Page Faults (Disk),” and “Page Faults (Zeroed)” should be equal to “TLB Faults.” 

So this means that you should not count TLB faults that do not get handled (i.e., result in the program being killed).
The code for printing out stats will print a warning if this these equalities do not hold.
In addition the sum of “Page Faults from ELF” and “Page Faults from Swapfile” should be equal to “Page Faults (Disk)”. 
When it is shut down (e.g., in vm_shutdown), your kernel should display the statistics it has gathered. */

#define NUM_OF_STATS 10

void vmstats_init(void);
void vmstats_print(void);

int vmstats_increase(unsigned int stat);

/* "Optimized" function to increase two linked stats, e.g., tlb faults and tlb faults with free*/
/* (Simply allows us to avoid useless spinlock release and acquire, unique critical section) */
int vmstats_increase_2(unsigned int stat1,unsigned int stat2);

#endif