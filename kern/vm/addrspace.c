/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <machine/vm.h>

#include <segment.h>
#include <novavm.h>
#include <coremap.h>
#include <pt.h>
#include <vmstats.h>

#include <spl.h>

#include "opt-novavm.h"

#include <mips/tlb.h>


/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	as->code=segment_create();
	as->data=segment_create();
	as->stack=segment_create();

	if (as->code == NULL || as->data == NULL || as->stack == NULL) {
        as_destroy(as);
        return NULL;
    }

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	novavm_can_sleep();

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	if (segment_copy(old->code,&(newas->code)) != 0) {return ENOMEM;}
	if (segment_copy(old->data, &(newas->data)) != 0) {return ENOMEM;}
	if (segment_copy(old->stack, &(newas->stack)) != 0) {return ENOMEM;}


	if (as_prepare_load(newas)) {
		as_destroy(newas);
		return ENOMEM;
	}

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	novavm_can_sleep();
	
	free_kpages(as->code->vaddr);
	free_kpages(as->data->vaddr);
	free_kpages(as->stack->vaddr);
	
	segment_destroy(as->code);
	segment_destroy(as->data);
	segment_destroy(as->stack);

	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;
	int spl,i;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	/* Our choice was to keep the flush of TLB during context-switch,
	which includes a call to as_activate */
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	vmstats_increase(TLB_INVALIDATIONS);
	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable, struct vnode *v,
		 uint32_t filesize, uint32_t offset)
{
	size_t npages;

	novavm_can_sleep();

	/* Align the region. First, the base... */
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = memsize / PAGE_SIZE;

	/* Text segment is read-only (S_RO)*/
	if (as->code->vaddr==0){
		as->code->vaddr=vaddr;
		as->code->as= as;
		as->code->memsize=memsize;
		as->code->npage=npages;
		as->code->permission=S_RO;
		as->code->file_elf=v;
		as->code->filesize=filesize;
		as->code->offset=offset;
		return 0;
	}

	if (as->data->vaddr==0){
		as->data->vaddr=vaddr;
		as->data->as=as;
		as->data->memsize=memsize;
		as->data->npage=npages;
		as->code->permission=S_RW;
		as->code->file_elf=v;
		as->code->filesize=filesize;
		as->code->offset=offset;
		return 0;
	}

	(void)readable;
	(void)writeable;
	(void)executable;
	

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("Warning [addrspace.c]: too many regions\n");
	return ENOSYS;
}

int
as_prepare_load(struct addrspace *as)
{

	(void)as;
	paddr_t p1, p2;

	/*
	KASSERT(as->code->vaddr == 0);
	KASSERT(as->data->vaddr == 0);
	KASSERT(as->stack->vaddr == 0);
	*/

	novavm_can_sleep();
	
	p1=segment_prepare_load(as->code);
	if (p1==0) {
		return ENOMEM;
	}
	p2=segment_prepare_load(as->data);
	if (p2==0) {
		return ENOMEM;
	}
	/*
	p3=segment_prepare_load(as->stack);
	if (p3==0) {
		return ENOMEM;
	}
	*/


	/*
	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
	*/

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	novavm_can_sleep();

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{

#if OPT_NOVAVM
	
	as->stack = segment_create();
	if (as->stack==NULL){
		return ENOMEM;
	}
	
	if (as->stack->vaddr==0){
		as->stack->vaddr=USERSTACK-(NOVAVM_STACKPAGES*PAGE_SIZE);
		as->stack->as=as;
		as->stack->memsize=NOVAVM_STACKPAGES*PAGE_SIZE;
		as->stack->npage=NOVAVM_STACKPAGES;
		as->stack->permission=S_RW;
		as->stack->file_elf=NULL;
		as->stack->filesize=0;
		as->stack->offset=0;
	}
	

	*stackptr = USERSTACK;
	return 0;
#else
	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
#endif
}

