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
#include <spl.h>
#include <mips/tlb.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */


struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->regions = NULL; /* At start no regions in addrespace */

	/* Create page directory */
	as->pd = kmalloc(sizeof(struct pagedirectory));
	if(as->pd == NULL) {
		kfree(as);
		return NULL;
	}

	/* Initialize page directory */
	as->pd->pagetables = kmalloc(sizeof(struct pagetable *) * PAGE_TABLE_ENTRIES);
	for(int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		as->pd->pagetables[i] = NULL;
	}

	return as;
}

void
as_destroy(struct addrspace *as)
{

	/* Free page tables */
	for(int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		if(as->pd->pagetables[i] != NULL) {
			kfree(as->pd->pagetables[i]);
		}
	}

	/* Free page directory */
	kfree(as->pd->pagetables);
	kfree(as->pd);

	struct region *region = as->regions;
	while(region != NULL) {
		struct region *next = region->next;
		kfree(region);
		region = next;
	}

	kfree(as);
}

void
as_activate(void)
{
	int i, spl;

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i); //Check this
	}

	splx(spl);
}

void
as_deactivate(void)
{
	int i, spl;

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i); //Check this
	}

	splx(spl);
}

/* Define a region in the address space 
*  The new region will be the head of the linked list of regions of this addrespace
*/
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	struct region *region = kmalloc(sizeof(struct region));
	if(region == NULL) {
		return ENOMEM;
	}

	region->vbase = vaddr;
	region->npages = npages;
	region->readable = readable;
	region->writeable = writeable;
	region->og_writeable = writeable;
	region->executable = executable;
	region->next = as->regions;

	as->regions = region;

	return 0;
}

/* Make read only regions into writable */
int
as_prepare_load(struct addrspace *as)
{
	struct region *region = as->regions;
	while(region != NULL) {
		if(region->writeable == 0) {
			region->writeable = 1;
		}
		region = region->next;
	}
	return 0;
}

/* Restore read only regions state */
int
as_complete_load(struct addrspace *as)
{
	struct region *region = as->regions;
	while(region != NULL) {
		if(region->og_writeable == 0) {
			region->writeable = 0;
		}
		region = region->next;
	}
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	*stackptr = USERSTACK;

	int result = as_define_region(as, USERSTACK - STACK_SIZE, STACK_SIZE, 1, 1, 1);
    if (result) {
        return result;
    }
    

    return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	/* Copy regions by defining new regions on new addresspace*/
	struct region *region = old->regions;

	while(region != NULL) {
		int result = as_define_region(new, region->vbase, region->npages * PAGE_SIZE, region->readable, region->writeable, region->executable);
		if(result) {
			as_destroy(new);
			return result;
		}
		region = region->next;
	};

	/* Copy page directory */
	for(int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
		if(old->pd->pagetables[i] != NULL) {
			struct pagetable *new_pt = kmalloc(sizeof(struct pagetable));
			if(new_pt == NULL) {
				as_destroy(new);
				return ENOMEM;
			}

			for(int j = 0; j < PAGE_TABLE_ENTRIES; j++) {
				new_pt->entries[j] = old->pd->pagetables[i]->entries[j];
			}

			new->pd->pagetables[i] = new_pt;
		}
	}


	*ret = new;
	return 0;
}
