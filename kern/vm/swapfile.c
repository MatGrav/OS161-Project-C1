//#include <stdio.h>
//#include <stdlib.h>
#include <types.h>
#include <lib.h>
#include <vm.h>
#include <vfs.h>
#include <synch.h>
#include "pt.h"
#include <swapfile.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <vnode.h>


static struct vnode* swapfile = NULL;
//static size_t index = 0; why was this defined?


void swap_init(){
    int result;
    char sf_name[] = "swapfile.txt"; // Nome del file di swap

    result = vfs_open(sf_name, O_RDWR | O_CREAT, 0664, &swapfile);
    if (result) {
        panic("swapfile_init: vfs_open failed\n");
    }

}

void swap_clean_up(){
    KASSERT(swapfile!=NULL);

    vfs_close(swapfile);
}


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
    ku.uio_rw = UIO_WRITE;
    ku.uio_space = NULL;
    
    
    // TO DO: where does this swapfile_vnode come from ?
    //VOP_WRITE(swapfile_vnode, &ku);
    (void)ku; //temp fix
}

paddr_t swap_out(paddr_t paddr){
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
    ku.uio_rw = UIO_READ;
    ku.uio_space = NULL;

    VOP_READ(swapfile, &ku);

    return paddr;

}