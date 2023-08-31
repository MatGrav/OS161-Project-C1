#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>

#include <addrspace.h>
#include <novavm.h>
#include <coremap.h>
#include <pt.h>


static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;

static struct coremap* cmap = NULL;

static unsigned int nRamFrames = 0;

static int coremapActive = 0;

int get_nRamFrames(){  return nRamFrames;}

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

  cmap->entry = kmalloc(sizeof(struct coremap_entry)*nRamFrames);
  if (cmap->entry==NULL){
    kprintf("Non ho allocato la cmap entry\n");

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
    cmap->entry[i].num_assaddr = 0;
    cmap->entry[i].consec_pages = 0;   
  }
  /* Until now we allocated contiguously by calling the kmalloc which uses
  ram_stealmem since coremap is not active */
  paddr_t firstpaddr = ram_stealmem(0);
  for (i=0;i<nRamFrames; i++){
    if(i<firstpaddr/PAGE_SIZE){
        /* Protecting frames used by kernel until now*/
        cmap->entry[i].status = RESERVED;
        cmap->entry[i].consec_pages = UNDEF_CONSEC_PAGES;
    }
    else if(i == firstpaddr/PAGE_SIZE){
        /* firstpaddr may be in the middle of a frame partly used by the kernel,
        we allow internal frammentation to preserve the "kernel bytes" */
        if(firstpaddr % PAGE_SIZE != 0){
            firstpaddr = cmap->entry[i+1].frame_addr;
            cmap->entry[i].status = RESERVED;
            cmap->entry[i].consec_pages = UNDEF_CONSEC_PAGES;
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
  //kprintf("%d\n",get_nRamFrames());  
}

unsigned long get_np_sz(){
  spinlock_acquire(&coremap_lock);
  unsigned long sz = cmap->np_sz;
  spinlock_release(&coremap_lock);

  return sz;
}

void coremap_cleanup(){
    unsigned int i;
    for (i=0;i<nRamFrames; i++){
      kfree(cmap->entry[i].associated_addr);
    }
    kfree(cmap->entry);
    kfree(cmap->np);
    kfree(cmap);
}


static void rem_head(){
    if (cmap->np_sz > 0) {
        /*
        if(cmap->entry[np_head]->status==FREE){
          cmap->entry[np_head]->status=OCCUPIED;
        }
        */
        cmap->np_head = (cmap->np_head + 1) % cmap->np_capacity;
        cmap->np_sz--;
    }
}

static paddr_t 
getfreeppages(unsigned long npages) {
  paddr_t addr = 0;	
  long i, first, found, np = (long)npages;

  if (!isCoremapActive()) return 0; 
  spinlock_acquire(&coremap_lock);
  for (i=0,first=found=-1; i<(long) (cmap->size); i++) {
    if ( cmap->entry[i].status == FREE) {
      if (i==0 || (cmap->entry[i].status != FREE)) 
        first = i; /* set first free in an interval */   
      if (i-first+1 >= np) {
        found = first;
        break;
      }
    }
  }
	
  if (found>=0) {
    for (i=found; i<found+np; i++) {
      cmap->entry[i].status = OCCUPIED;
      cmap->entry[i].consec_pages = np;

      /* Free entries should also be removed from cmap np*/
      rem_head();
      /* TO DO*/
    }
    addr = (paddr_t) cmap->entry[found].frame_addr;
  }
  else {
    addr = 0;
  }

  spinlock_release(&coremap_lock);

  return addr;
}

static paddr_t getppages(unsigned long npages){
  paddr_t addr;

  /* try freed pages first */
  addr = getfreeppages(npages);
  if (addr == 0) {
    /* call stealmem, only when coremap is not active*/
    spinlock_acquire(&stealmem_lock);
    addr = ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
  }

  /* Status change taken care by getfreeppages*/
  return addr;
}

static int freeppages(paddr_t addr, unsigned long npages){
  long i, first, np=(long)npages;	

  if (!isCoremapActive()) return 0; 
  first = addr/PAGE_SIZE;
  KASSERT(cmap->entry!=NULL);
  KASSERT(cmap->size > (unsigned) first);

  spinlock_acquire(&coremap_lock);
  for (i=first; i<first+np; i++) {
    cmap->entry[i].status = FREE;
    cmap->entry[i].consec_pages = 0;
    kprintf("DEbug: CLeaning a page\n");
    /*Aggiungere alla lista dei np*/
  }
  spinlock_release(&coremap_lock);

  return 1;
}

vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	novavm_can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr){
  if (isCoremapActive()) {
    paddr_t paddr;
    //TO DO I think: Substitute the code with one asking page table the phys address
    paddr = addr - MIPS_KSEG0;
    /* If paddr is in overflow -> it is an user address */
    /*if(paddr >= cmap->size * PAGE_SIZE){
      paddr=pt_translate(addr);
    }*/
    
    if(addr <= 0x7FFFFFFF){
      paddr = pt_translate(addr);
    }
  
    long first = paddr/PAGE_SIZE;	
    KASSERT(cmap->entry!=NULL);
    KASSERT(cmap->size > (unsigned) first);

    spinlock_acquire(&coremap_lock);
    unsigned int npages = cmap->entry[paddr/PAGE_SIZE].consec_pages;
    spinlock_release(&coremap_lock);

    freeppages(paddr, npages);	
  }
}

paddr_t
alloc_upage(){

  paddr_t pa;

	novavm_can_sleep();
	pa = getppages(1);
	if (pa==0) {
		return 0;
	}
	return pa;

}

void
free_upage(paddr_t paddr){
  if (isCoremapActive()) {
    freeppages(paddr, 1);
  }
}