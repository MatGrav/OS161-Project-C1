#include <types.h>
#include <lib.h>
#include <vm.h>
#include <coremap.h>
#include <spinlock.h>
#include <swapfile.h>
#include <proc.h>

#include <novavm.h>
#include <machine/vm.h>

#include "ipt.h"

struct spinlock free_ipt = SPINLOCK_INITIALIZER;
struct spinlock free_queue = SPINLOCK_INITIALIZER;

/* Page Table */
struct ipt_entry *ipt = NULL;

/* **Queue for FIFO support** */
/* queue_fifo contains indexes (1,2,3...) of page table */
// scrivo in coda e prelevo in testa
unsigned int *queue_fifo = NULL;
static unsigned int queue_front = 0;
static unsigned int queue_rear = 0;

void ipt_init()
{
    ipt = (struct ipt_entry *)kmalloc(sizeof(struct ipt_entry) * IPT_SIZE);
    if (ipt == NULL)
    {
        return;
    }
    ipt_clean_up();

    queue_fifo = kmalloc(sizeof(uint8_t) * IPT_SIZE);
    if (queue_fifo == NULL)
    {
        return;
    }

#if PRINT_IPT_DIM
    kprintf("Dimension of a single IPT entry: %d B\n", sizeof(struct ipt_entry));
    kprintf("Current IPT_SIZE: %d (# of frames/pages)\n", IPT_SIZE);
    kprintf("Dimension of page table %d KB \n", (sizeof(struct ipt_entry) * IPT_SIZE) / 1024);
    kprintf("Size of queue fifo %d KB \n\n", (sizeof(unsigned int) * IPT_SIZE) / 1024);
#endif

    for (unsigned long i = 0; i < IPT_SIZE; i++)
    {
        queue_fifo[i] = 0;
    }
}

/* Pop on queue_fifo */
/* Push on queue_fifo is done in ipt_map */
static unsigned int ipt_queue_fifo_pop()
{
    KASSERT(queue_front != queue_rear);

    spinlock_acquire(&free_queue);

    /* index of old page to pop */
    unsigned int old = queue_fifo[queue_front];
    spinlock_release(&free_queue);
    // write on swapfile -> call a function in swapfile.c
    ipt_swap_push(&ipt[old]);
    spinlock_acquire(&free_queue);
    ipt_page_free(old);
    queue_front = (queue_front + 1) % IPT_SIZE;

    spinlock_release(&free_queue);

    return old;
}

static void ipt_queue_fifo_push(unsigned int el)
{
    KASSERT(el != 0);

    spinlock_acquire(&free_queue);
    queue_fifo[queue_rear] = i; /* we write the index of page table */
    queue_rear = (queue_rear + 1) % IPT_SIZE; /* update of rear */
    spinlock_release(&free_queue);
    
    /* pop on swapfile if paddr is written there */
    /* pop = invalidate the page entry in SWAPFILE */    
    ipt_swap_pop(&ipt[i]);

}

void ipt_clean_up()
{
    unsigned long i;
    for (i = 0; i < IPT_SIZE; i++)
    {
        ipt[i].p_pid = 0;
        ipt[i].page_number = 0;
        ipt[i].status = ABSENT;
        ipt[i].protection = IPT_E_RW;
    }
}

void ipt_page_free(unsigned int i)
{
    ipt[i].p_pid = 0;
    ipt[i].page_number = 0;
    ipt[i].status = ABSENT;
    ipt[i].protection = IPT_E_RW;
}

void ipt_destroy()
{
    kfree(ipt);
}

static unsigned ipt_hash(vaddr_t v)
{
    const double A = (2.236067 - 1.0) / 2.0; // Costante di Knuth A (0.6180339887...)
    double fractional_part = A * (double)v;
    int index = (int)(IPT_SIZE * (fractional_part - (int)fractional_part));

    if (index < 0)
    {
        index += IPT_SIZE;
    }

    return index;
}

/* Returns index where virtual address is, found is to be checked after calling */
static unsigned int ipt_search(pid_t pid, vaddr_t v, bool *found)
{
    KASSERT(pid != 0);
    KASSERT(v != 0);

    *found = false;
    /* Page number, no displacement */
    v &= PAGE_FRAME;

    spinlock_acquire(&free_ipt);
    /* index to access to inverted page table */
    unsigned int index = ipt_hash(v);

    if (ipt[index].p_pid == pid && ipt[index].page_number == v)
    {
        *found = true;
    }
    else
    {
        unsigned int i;
        for (i = index; i < IPT_SIZE; i++)
        {
            if (ipt[i].p_pid == pid && ipt[i].page_number == v)
            {
                *found = true;
                index = i;
                break;
            }
        }
        if (!found)
        {
            for (i = 0; i < IPT_SIZE - index; i++)
            {
                if (ipt[i].p_pid == pid && ipt[i].page_number == v)
                {
                    *found = true;
                    index = i;
                    break;
                }
            }
        }
    }

    KASSERT(ipt[index].status == PRESENT);
    KASSERT(ipt[index].protection == IPT_E_RW);

    spinlock_release(&free_ipt);

    return index;
}

void ipt_map(pid_t pid, vaddr_t v, paddr_t p)
{

    KASSERT(pid != 0);
    KASSERT(v != 0);

    /* To be sure it is aligned */
    /* Page number */
    v &= PAGE_FRAME;
    /* To be sure it is aligned */
    p &= PAGE_FRAME;

    /* Index in IPT based on paddr */
    unsigned int i = (unsigned int)p / PAGE_SIZE;

    spinlock_acquire(&free_ipt);
    if (ipt[i].status == ABSENT)
    {
        ipt[i].p_pid = pid;
        ipt[i].page_number = v;
        ipt[i].status = PRESENT;
        ipt[i].protection = IPT_E_RW;
    }
    else
    {
        panic("Hashing collision!\n");
    }
    spinlock_release(&free_ipt);

    /* TO DO: Should be checked for IPT */
    /* Push on queue_fifo and check on swapfile */
    ipt_queue_fifo_push(i);

    (void)res;
}

paddr_t ipt_fault(uint32_t faulttype)
{
    unsigned int i;
    switch (faulttype)
    {
    case INVALID_MAP:
        panic("Invalid input address for paging\n");

    case NOT_MAPPED:
    {
        /* Wr're trying to access to a not mapped page */
        /* Let's update it in memory and so in page table */

        paddr_t p = alloc_upage();

        if (p == 0)
        {
            /* there's not enough space -> substitute */
            /* PAGE REPLACEMENT */
            i = ipt_queue_fifo_pop();  // Liberazione nella pt
            free_upage(i * PAGE_SIZE); // Liberazione nella "coremap"
            p = alloc_upage();         /* new address -> later, look if it is written in swapfile*/
        }

        /* Remember: p must be mapped -> look at ipt_translate() */
        return p;
    }
    break;
    default:
        break;
    }
    return 0;
}

paddr_t ipt_translate(pid_t pid, vaddr_t v)
{
    unsigned i;
    // pid_t pid = proc_getpid();
    KASSERT(pid != 0);

    /* Alignment of virtual address to page */
    /* Page number */
    v &= PAGE_FRAME;

    if (v >= USERSTACK)
    {
        ipt_fault(INVALID_MAP);
    }

    /* Search of the virtual address inside the IPT */
    bool found = false;
    i = ipt_search(pid, v, &found);

    /* frame number */
    paddr_t p;

    if (found)
    {
        p = PAGE_SIZE * i;
    }
    else
    {
        p = ipt_fault(NOT_MAPPED);
        ipt_map(pid, v, p);
    }

    return p;
}

void ipt_swap_push(struct ipt_entry *ipt_e)
{
    KASSERT(ipt_e->p_pid!=0);
    KASSERT(ipt_e->page_number!=0);
    KASSERT(ipt_e->protection==IPT_E_RW);

    paddr_t p = ipt_translate(ipt_e->p_pid, ipt_e->page_number);

    swap_in(p);

}

/* This just invalidates the ipt_entry in SWAPFILE. */
/* If it is not present the ipt_entry, don't care. */
void ipt_swap_pop(struct ipt_entry *ipt_e)
{
    KASSERT(ipt_e->p_pid!=0);
    KASSERT(ipt_e->page_number!=0);
    KASSERT(ipt_e->protection==IPT_E_RW);

    paddr_t p = ipt_translate(ipt_e->p_pid, ipt_e->page_number);
    
    swap_out(p);

    return res;
}