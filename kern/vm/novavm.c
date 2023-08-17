#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

#include <novavm.h>
#include <coremap.h>
#include <pt.h>

/* Initialization function */
void vm_bootstrap(void){
	coremap_init();
	pt_init();
}

void
novavm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

/* Fault handling function called by trap code */
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	//vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "novavm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
		// TO DO
	    case VM_FAULT_READONLY:
			/* We always create pages read-write, so we can't get this */
			panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
			/*Should do something?*/
			break;
	    case VM_FAULT_WRITE:
			/*Should do something?*/
			break;
	    default:
			return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	/* TO DO
	
	 |
	 |
	 v
	
	I think we should change addrspace to have pointers to segments */
	KASSERT(as->code  != NULL);
	KASSERT(as->data  != NULL);
	KASSERT(as->stack != NULL);

	KASSERT((as->code->vaddr & PAGE_FRAME) == as->code->vaddr);
	KASSERT((as->data->vaddr & PAGE_FRAME) == as->data->vaddr);
	KASSERT((as->stack->vaddr & PAGE_FRAME) == as->data->vaddr);

	//TO DO:
	// paddr = pt_get_page? (faultaddress)

	if(paddr == NULL){
		return EFAULT;
	}

	/* 
	SHOULD BE ERASED?

	vbase1 = as->code.vaddr;
	vtop1 = vbase1 + as->as_npage * PAGE_SIZE;

	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + pt_get_page(as->code.vaddr);
	}

	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + pt_get_page(as->data.vaddr);
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + pt_get_page(as->stack.vaddr);
	}
	else {
		return EFAULT;
	}
	*/


	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "novavm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	/* At this point dumbvm, running out of entries, couldn't handle pf
	returning EFAULT*/
	tlb_write(ehi,elo,tlb_get_rr_victim());
	splx(spl);
	return 0;


	kprintf("novavm: We shouldn't have reached this instruction...\n");
	splx(spl);
	return EFAULT;
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *ts){
    
	(void)ts;

}



