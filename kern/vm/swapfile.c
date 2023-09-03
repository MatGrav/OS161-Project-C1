#include <types.h>
#include <lib.h>
#include <vm.h>
#include <vfs.h>
#include <synch.h>
#include <ipt.h>
#include <swapfile.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <vnode.h>
#include <spinlock.h>
#include <kern/errno.h>

#include <vmstats.h>


static struct vnode* swapfile = NULL;
static unsigned int* bitmap = NULL; 

static struct spinlock swap_free = SPINLOCK_INITIALIZER;


void swap_init(){
    int result, i;
    
    char sf_name[] = "emu0:/SWAPFILE"; // Nome del file di swap

    result = vfs_open(sf_name, O_RDWR | O_CREAT, 0, &swapfile);
    if (result) {
        panic("swapfile_init: vfs_open failed\n");
    }

    result = VOP_TRUNCATE(swapfile, SWAPFILE_SIZE);
    if (result) {
        panic("swapfile_init: VOP_TRUNCATE unable to set swapfile size\n");
    }

    bitmap = kmalloc(sizeof(unsigned int) * SWAPFILE_SIZE/PAGE_SIZE);
    if(bitmap == NULL){
        panic("swap_init: unable to allocate bitmap\n");
        return;
    }
    for(i=0; i<SWAPFILE_SIZE/PAGE_SIZE; i++){
        bitmap[i]=SF_ABSENT;
    }
}

void swap_clean_up(){
    KASSERT(swapfile!=NULL);

    kfree(bitmap);
    vfs_close(swapfile);
}


void swap_out(paddr_t paddr){
    KASSERT(swapfile!=NULL);

    struct iovec iov;
    struct uio ku;
    size_t index;

    index=paddr/PAGE_SIZE;
    
    iov.iov_ubase = (userptr_t)&paddr;
    iov.iov_len = sizeof(paddr_t);
    
    ku.uio_iov = &iov;
    ku.uio_iovcnt = 1;
    ku.uio_offset = index * sizeof(paddr_t);
    ku.uio_resid = sizeof(paddr_t);
    ku.uio_segflg = UIO_USERSPACE;
    ku.uio_rw = UIO_WRITE; /* from uio_seg to kernel */
    ku.uio_space = NULL;
    
    spinlock_acquire(&swap_free);
    if(bitmap[index]==SF_ABSENT){
        VOP_WRITE(swapfile, &ku);
        bitmap[index]=SF_PRESENT;
    }
    spinlock_release(&swap_free);
    vmstats_increase(SWAPFILE_WRITES);
    
}

/* Useful because it includes a "swap_search" */
/* Every page is written with a specific offset in SWAPFILE */
/* So, we can quickly know if a page is written (or not) in SWAPFILE */
void swap_in(paddr_t paddr){
    KASSERT(swapfile!=NULL);

    struct iovec iov;
    struct uio ku;
    size_t index;

    index=paddr/PAGE_SIZE;

    iov.iov_ubase = (userptr_t)&paddr;
    iov.iov_len = sizeof(paddr_t);

    ku.uio_iov = &iov;
    ku.uio_iovcnt = 1;
    ku.uio_offset = index * sizeof(paddr_t);
    ku.uio_resid = sizeof(paddr_t);
    ku.uio_segflg = UIO_USERSPACE;
    ku.uio_rw = UIO_READ; /* from kernel to uio_seg */
    ku.uio_space = NULL;


    spinlock_acquire(&swap_free);
    if(bitmap[index]==SF_PRESENT) {
        VOP_READ(swapfile, &ku);
        bitmap[index]=SF_ABSENT;
    } else {
        spinlock_release(&swap_free);
        return;
    }
    spinlock_release(&swap_free);
    vmstats_increase(PAGE_FAULTS_SWAPFILE);


    return;
}