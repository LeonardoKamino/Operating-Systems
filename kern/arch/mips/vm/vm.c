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
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
struct coremap cm;
volatile int coremap_pages;
struct spinlock cm_lock;


void coremap_init()
{
	// cm = kmalloc(sizeof(cm));	
	paddr_t ramsize = ram_getsize();

	coremap_pages = ramsize / PAGE_SIZE;
	

	paddr_t firstpaddr = ram_getfirstfree();

	cm.cm_entries = (struct cm_entry *) PADDR_TO_KVADDR(firstpaddr);

	spinlock_init(&cm_lock);

	struct cm_entry cm_entry;
	for (int i = 0; i < coremap_pages; i++)
	{
		cm_entry.is_free = true;
		cm_entry.is_end_malloc = true;
		memmove(&cm.cm_entries[i], &cm_entry, sizeof(cm_entry));
	}


	uint32_t i;
	for (i = 0; i < (firstpaddr + (coremap_pages * sizeof(struct cm_entry))) / PAGE_SIZE + 1; i++)
	{
		cm.cm_entries[i].is_free = false;
	}
}

void vm_bootstrap(void)
{
	/* Do nothing. */
	coremap_init();
}

paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}

int find_free_space(int npages)
{
	int free_pages = 0;
	int start_index = -1;
	for (int i = 0; i < coremap_pages; i++)
	{
		if (cm.cm_entries[i].is_free)
		{
			if (start_index == -1)
			{
				start_index = i;
			}
			free_pages++;
			if (free_pages == npages)
			{
				return start_index;
			}
		}
		else
		{
			free_pages = 0;
			start_index = -1;
		}
	}
	return -1;
} 

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{


	paddr_t paddr;
	if (cm.cm_entries == NULL)
	{
		spinlock_acquire(&stealmem_lock);
		paddr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
	}
	else
	{
		spinlock_acquire(&cm_lock);

		int first_free = find_free_space(npages);

		if (first_free == -1)
		{
			spinlock_release(&cm_lock);
			return 0;
		}

		paddr = first_free * PAGE_SIZE;

		for (int i = first_free; i < first_free + (int) npages; i++)
		{
			cm.cm_entries[i].is_free = false;
			cm.cm_entries[i].is_end_malloc = false;
		}

		cm.cm_entries[first_free + (int) npages - 1].is_end_malloc = true;

		spinlock_release(&cm_lock);
	}

	bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);

	return PADDR_TO_KVADDR(paddr);
}

void free_kpages(vaddr_t addr)
{
	paddr_t paddr = KVADDR_TO_PADDR(addr);

	spinlock_acquire(&cm_lock);

	int index = paddr / PAGE_SIZE;

	while(!cm.cm_entries[index].is_free)
	{
		cm.cm_entries[index].is_free = true;
		if(cm.cm_entries[index].is_end_malloc)
		{
			break;
		}
		index++;
	}

	spinlock_release(&cm_lock);
}

void vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype)
	{
	case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
	case VM_FAULT_READ:
	case VM_FAULT_WRITE:
		break;
	default:
		return EINVAL;
	}

	if (curproc == NULL)
	{
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL)
	{
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	// Extract the 10 most significant bits (bits 22-31)
	unsigned int msb = (faultaddress >> 22) & 0x3FF; // 0x3FF is the mask for 10 bits

	// Extract bits 11-20
	unsigned int mid = (faultaddress >> 12) & 0x3FF; // Shift right by 12 and mask

	struct pagedirectory *pd = as->pd;

	if (pd->pagetables[msb] == NULL)
	{
		pd->pagetables[msb] = kmalloc(sizeof(struct pagetable));
		if (pd->pagetables[msb] == NULL)
		{
			return ENOMEM;
		}

		for (int i = 0; i < PAGE_TABLE_ENTRIES; i++)
		{
			pd->pagetables[msb]->entries[i] = 0;
		}
	}

	if (pd->pagetables[msb]->entries[mid] == 0)
	{
		struct region *region = as->regions;

		while (region != NULL)
		{
			if (faultaddress >= region->vbase && faultaddress <= (region->vbase + region->npages * PAGE_SIZE))
			{
				vaddr_t new_page = alloc_kpages(1);
				if (new_page == 0)
				{
					return ENOMEM;
				}
				if (region->writeable)
				{
					pd->pagetables[msb]->entries[mid] = PADDR_TO_KVADDR(new_page & PAGE_FRAME) | TLBLO_DIRTY | TLBLO_VALID;
				}
				else
				{
					pd->pagetables[msb]->entries[mid] = PADDR_TO_KVADDR(new_page & PAGE_FRAME) | 0 | TLBLO_VALID;
				}
				paddr = pd->pagetables[msb]->entries[mid];
				region = NULL;
			}
			else
			{
				region = region->next;
			}
		}

		if (pd->pagetables[msb]->entries[mid] == 0)
		{
			return EFAULT;
		}
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++)
	{
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID)
		{
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
}