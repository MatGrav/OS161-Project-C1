#include "vmstats.h"
#include <types.h>
#include <spinlock.h>
#include <lib.h>


static uint32_t vmstats[NUM_OF_STATS];

static struct spinlock vmstats_lock = SPINLOCK_INITIALIZER;

static int vmstatsActive = 0;

static int isVmstatsActive(){
    int active;
    spinlock_acquire(&vmstats_lock);
    active = vmstatsActive;
    spinlock_release(&vmstats_lock);
    return active;
}

void vmstats_init(){
    for(int i=0;i<NUM_OF_STATS;i++){
        vmstats[i]=0;
    }
    spinlock_acquire(&vmstats_lock);
    vmstatsActive = 1;
    spinlock_release(&vmstats_lock);
}

void vmstats_print(){

    /* This functions is meant to be called during vm_shutdown, 
    however is written "correctly" at any time (I/O is expensive, so we shorten critical section) */
    spinlock_acquire(&vmstats_lock);
    uint32_t curstats[NUM_OF_STATS];
    for(int i=0;i<NUM_OF_STATS;i++){
        curstats[i] = vmstats[i];
    }
    spinlock_release(&vmstats_lock);


    kprintf("Printing vm stats:\n");
    if(curstats[TLB_FAULTS] != (curstats[TLB_FAULTS_FREE] + curstats[TLB_FAULTS_REPLACE])){
        kprintf("[WARNING: TLB_FAULTS is not equal to TLB_FAULT_FREE + TLB_FAULTS_REPLACE]\n");
    };
    kprintf("TLB Faults: %u\n",curstats[TLB_FAULTS]);
    kprintf("TLB Faults with Free: %u\n",curstats[TLB_FAULTS_FREE]);
    kprintf("TLB Faults with Replace: %u\n",curstats[TLB_FAULTS_REPLACE]);

    kprintf("TLB Invalidations: %u\n",curstats[TLB_INVALIDATIONS]);
    kprintf("TLB Reloads: %u\n",curstats[TLB_RELOADS]);

    if(curstats[TLB_FAULTS] != (curstats[TLB_RELOADS] + curstats[PAGE_FAULTS_DISK] + curstats[PAGE_FAULTS_ZEROED])){
        kprintf("\n[WARNING: TLB Fault is not equal to TLB Reloads + PageFaults (Disk) + PageFaults(Zeroed)]\n");
    };

    kprintf("Page Faults (Zeroed): %u\n",curstats[PAGE_FAULTS_ZEROED]);
    kprintf("Page Faults (Disk): %u\n",curstats[PAGE_FAULTS_DISK]);
    kprintf("Page Faults from ELF: %u\n",curstats[PAGE_FAULTS_ELF]);
    kprintf("Page Faults from Swapfile: %u\n",curstats[PAGE_FAULTS_SWAPFILE]);
    kprintf("Swapfile Writes: %u\n",curstats[SWAPFILE_WRITES]);

    if(curstats[TLB_FAULTS] != (curstats[PAGE_FAULTS_DISK] + curstats[PAGE_FAULTS_SWAPFILE] + curstats[PAGE_FAULTS_ELF])){
        kprintf("\n[WARNING: Page Faults (Disk) is not equal to Page Faults from Swapfile + Page Faults from ELF]\n");
    };
}

int vmstats_increase(unsigned int stat){

    if(stat >= NUM_OF_STATS){
        return E_STATS_OVER;
    }
    if(isVmstatsActive() == 0){
        return E_STATS_INACTIVE;
    }

    spinlock_acquire(&vmstats_lock);
    vmstats[stat] += 1;
    spinlock_release(&vmstats_lock);
    return 0;
}


int vmstats_increase_2(unsigned int stat1, unsigned int stat2){

    if(stat1 >= NUM_OF_STATS || stat2 >= NUM_OF_STATS){
        return E_STATS_OVER;
    }
    if(isVmstatsActive() == 0){
        return E_STATS_INACTIVE;
    }

    spinlock_acquire(&vmstats_lock);
    vmstats[stat1] += 1;
    vmstats[stat2] += 1;
    spinlock_release(&vmstats_lock);
    return 0;
}