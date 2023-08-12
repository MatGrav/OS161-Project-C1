/* #include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <spinlock.h> */

/*
#include <types.h>
#include <spinlock.h>
#include <lib.h>


#include <kern/errno.h>

#include <spl.h>
#include <cpu.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <novavm.h>

#include <coremap.h>
*/
#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <proc.h>

#include <addrspace.h>
#include <novavm.h>
#include <coremap.h>



static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

static struct coremap* cmap = NULL;

static unsigned int nRamFrames = 0;

static int coremapActive = 0;

static int isCoremapActive(){
    int active;
    spinlock_acquire(&coremap_lock);
    active = coremapActive;
    spinlock_release(&coremap_lock);
    return active;
}

void coremap_init(){
  unsigned int i;
  nRamFrames = ((int)ram_getsize())/PAGE_SIZE;

  cmap = kmalloc(sizeof(struct coremap));
  
  if (cmap==NULL){
    kprintf("Non ho allocato la coremap\n");
    return;
  }  


  /* alloc freeRamFrame and allocSize */  
  cmap->entry = kmalloc(sizeof(struct coremap_entry)*nRamFrames);
  if (cmap->entry==NULL){
    kprintf("Non ho allocato la cmap enttry\n");

    kfree(cmap);
    return;
  } 
  cmap->np = kmalloc(sizeof(struct coremap_entry*)*nRamFrames);
  if (cmap->np==NULL) {    
    kfree(cmap->entry);
    kfree(cmap);
    return;
  }
  cmap->size = nRamFrames;
  

  for (i=0; i<nRamFrames; i++) {    
    cmap->entry[i].associated_addr = kmalloc(sizeof(struct addrspace*)*MAX_NUM_ASS_ADDR);
    cmap->entry[i].num_assaddr = 0;
    cmap->entry[i].frame_addr = i*PAGE_SIZE;   
  }
  /* Until now we allocated contiguously by calling the kmalloc which uses
  ram_stealmem since coremap is not active */
  paddr_t firstpaddr = ram_stealmem(0);
  for (i=0;i<nRamFrames; i++){
    if(i<firstpaddr/PAGE_SIZE){
        /* Protecting frames used by kernel until now*/
        cmap->entry[i].status = RESERVED;

    }
    else if(i == firstpaddr/PAGE_SIZE){
        /* firstpaddr may be in the middle of a frame partly used by the kernel,
        we allow internal frammentation to preserve the "kernel bytes" */
        if(firstpaddr % PAGE_SIZE != 0){
            firstpaddr = cmap->entry[i+1].frame_addr;
            cmap->entry[i].status = RESERVED;
        }
        else{
            cmap->entry[i].status = FREE;
        }

    }else{    
        cmap->entry[i].status = FREE;
    }
  }

  /* At this point, firstpaddr always points to the start of a null frame*/
  for (i=0;i<(nRamFrames-firstpaddr/PAGE_SIZE); i++){
    cmap->np[i] = &(cmap->entry[i+firstpaddr/PAGE_SIZE]);
  }
  cmap->np_head = 0;
  cmap->np_tail = nRamFrames-firstpaddr/PAGE_SIZE-1;
  cmap->np_sz = nRamFrames-firstpaddr/PAGE_SIZE;
  cmap->np_capacity = nRamFrames-firstpaddr/PAGE_SIZE;


  spinlock_acquire(&coremap_lock);
  coremapActive = 1;
  spinlock_release(&coremap_lock);    
}


/* Should never be called?*/
void coremap_cleanup(){
    unsigned int i;
    for (i=0;i<nRamFrames; i++){
      kfree(cmap->entry[i].associated_addr);
    }
    kfree(cmap->entry);
    kfree(cmap->np);
    kfree(cmap);
}


void rem_head(){
    if (cmap->np_sz > 0) {
        cmap->np_head = (cmap->np_head + 1) % cmap->np_capacity;
        cmap->np_sz--;
    }
}


static int
getfreeppages(unsigned long npages, paddr_t addr[]) {
  unsigned long found = (long) 0;

  if (!isCoremapActive()) return 0; 
  spinlock_acquire(&coremap_lock);

  for(found=0;found<npages;found++){

    if(cmap->np_sz > 0){
        addr[found] = cmap->np[cmap->np_head]->frame_addr;
        cmap->np[cmap->np_head]->status = OCCUPIED;
        rem_head();
        found++;
    } else {
        /* SOSTITUZIONE*/
        /* Vedere cosa succede per la page table quando sarà implementata*/
        /* Ricordiamoci di mettere anche in questo caso lo status OCCUPIED*/
    }
  }

  return (int) found;
}


static paddr_t* getppages(unsigned long npages, paddr_t addr[]){
  //paddr_t addr[npages];

  /* try freed pages first */
  int res = getfreeppages(npages,addr);
  if (res == 0) {
    /* call stealmem, only when coremap is not active*/
    spinlock_acquire(&stealmem_lock);
    addr[0] = ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
  }

  /* Status change taken care by getfreeppages*/
  return addr;
}

static int freeppages(paddr_t e1, unsigned long e2){
  (void) e1;
  (void) e2;
  panic("something freeppages");
}

vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa[npages];

	novavm_can_sleep();
	getppages(npages,pa);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa[0]);
}

void free_kpages(vaddr_t T){
  (void)T;
  
  
  panic("Should not enter here");

  freeppages(0,0);
}