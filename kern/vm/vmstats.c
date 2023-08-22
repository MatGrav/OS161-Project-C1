#include "vmstats.h"
#include <types.h>
#include <spinlock.h>


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
    spinlock_acquire(&vmstats_lock);
    for(int i=0;i<NUM_OF_STATS;i++){
        vmstats[i]=0;
    }
    vmstatsActive = 1;
    spinlock_release(&vmstats_lock);
}

void vmstats_print(){

    kprintf("Printing vm stats:\n");

    if(vmstats[TLB_FAULTS] != (vmstats[TLB_FAULTS_FREE] + vmstats[TLB_FAULTS_REPLACE])){
        kprintf("[WARNING: TLB_FAULTS is not equal to TLB_FAULT_FREE + TLB_FAULTS_REPLACE]\n");
    };
    kprintf("TLB Faults: %u\n",vmstats[TLB_FAULTS]);
    kprintf("TLB Faults with Free: %u\n",vmstats[TLB_FAULTS_FREE]);
    kprintf("TLB Faults with Replace: %u\n",vmstats[TLB_FAULTS_REPLACE]);

    kprintf("TLB Invalidations: %u\n",vmstats[TLB_INVALIDATIONS]);
    kprintf("TLB Reloads: %u\n",vmstats[TLB_RELOADS]);

    if(vmstats[TLB_FAULTS] != (vmstats[TLB_RELOADS] + vmstats[PAGE_FAULTS_DISK] + vmstats[PAGE_FAULTS_ZEROED])){
        kprintf("\n[WARNING: TLB Fault is not equal to TLB Reloads + PageFaults (Disk) + PageFaults(Zeroed)]\n");
    };

    kprintf("Page Faults (Zeroed): %u\n",vmstats[PAGE_FAULTS_ZEROED]);
    kprintf("Page Faults (Disk): %u\n",vmstats[PAGE_FAULTS_DISK]);
    kprintf("Page Faults from ELF: %u\n",vmstats[PAGE_FAULTS_ELF]);
    kprintf("Page Faults from Swapfile: %u\n",vmstats[PAGE_FAULTS_SWAPFILE]);
    kprintf("Swapfile Writes: %u\n",vmstats[SWAPFILE_WRITES]);

    if(vmstats[TLB_FAULTS] != (vmstats[PAGE_FAULTS_DISK] + vmstats[PAGE_FAULTS_SWAPFILE] + vmstats[PAGE_FAULTS_ELF])){
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