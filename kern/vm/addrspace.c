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

#include <segment.h>
#include <novavm.h>
#include <pt.h>

#include <spl.h>

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

	newas->code = segment_copy(old->code);
	newas->data = segment_copy(old->data);
	newas->stack = segment_copy(old->stack);


	if (as_prepare_load(newas)) {
		as_destroy(newas);
		return ENOMEM;
	}

	KASSERT(pt_get_page(new->code.vaddr) != 0);
	KASSERT(pt_get_page(new->data.vaddr) != 0);
	KASSERT(pt_get_page(new->stack.vaddr) != 0);

	/*
	???????????????????????????
	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
	*/

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	novavm_can_sleep();

	freeppages(pt_get_page(as->code.vaddr), as->code.npage);
	freeppages(pt_get_page(as->data.vaddr), as->data.npage);
	freeppages(pt_get_page(as->stack.vaddr), as->stack.npage);

	kfree(as->code);
	kfree(as->data);
	kfree(as->stack);
	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

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
		 int readable, int writeable, int executable)
{
	size_t npages;

	novavm_can_sleep();

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	if (as->code.vaddr==0){
		as->code.vaddr=vaddr;
		as->code.as= &as;
		as->code.memsize=memsize;
		as->code.npage=npages;
		//as->code.permission
		//as->code.file_elf
		//as->code.filesize
		//as->code.offset
		return 0;
	}

	if (as->data.vaddr==0){
		as->data.vaddr=vaddr;
		as->data.as=as;
		as->data.memsize=memsize;
		as->data.npage=npages;
		//as->code.permission
		//as->code.file_elf
		//as->code.filesize
		//as->code.offset
		return 0;
	}

	/*
	
	Stack?

	if (as->stack.vaddr==0){
		as->stack.vaddr=vaddr;
		as->stack.as=as;
		as->stack.memsize=memsize;
		as->stack.npage=npages;
		//as->stack.permission
		//as->stack.file_elf
		//as->stack.filesize
		//as->stack.offset
		return 0;
	}
	*/


	(void)readable;
	(void)writeable;
	(void)executable;
	

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}

int
as_prepare_load(struct addrspace *as)
{
	paddr_t p1, p2, p3;
	KASSERT(pt_get_page(as->code.vaddr) == 0);
	KASSERT(pt_get_page(as->data.vaddr) == 0);
	KASSERT(pt_get_page(as->stack.vaddr) == 0);

	novavm_can_sleep();

	/* ATTENZIONE: l'address space deve lavorare con l'indirizzo virtuale, non fisico  */
	p1=getppages(as->code.npage);
	if (p1 == 0) {
		return ENOMEM;
	}
	p2=getppages(as->data.npage);
	if (p2 == 0) {
		return ENOMEM;
	}
	p3=getppages(as->stack.npage);
	if (p3 == 0) {
		return ENOMEM;
	}

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
	KASSERT(pt_get_page(as->stack.vaddr) != 0);
	//KASSERT(pt_get_page(as->stack->vaddr) != 0);


	/* Initial user-level stack pointer */
	// ??????????????
	*stackptr = USERSTACK;

	return 0;
}

