#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"

#include "../conc/kspinlock.h"


struct kspinlock kheap_lock;

//nouran
// kol el pages elle wara ba3d , ashan lma n allocate w n free
struct Bulk {
	uint32 start;
	uint32 size;
	struct Bulk* next;
	struct Bulk* prev;
};

//hamed
static struct Bulk* bulks_allocated=NULL;

//nouran
static struct Bulk* bulks = NULL; // pointer for first element in list

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
void kheap_init()
{

	init_kspinlock(&kheap_lock, "kheap_lock");
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
	}
	//==================================================================================
	//==================================================================================
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================

//extra function hamed fast allocation
int map_frame_range(uint32 va_start, uint32 size) {
    uint32 va = va_start;
    uint32 end_va = va_start + size;

    while (va < end_va) {
        struct FrameInfo* fi;
        int ret = allocate_frame(&fi);
        if (ret != E_NO_MEM) {
            ret = map_frame(ptr_page_directory, fi, va, PERM_WRITEABLE);
            if (ret == E_NO_MEM) {
                free_frame(fi);
                return E_NO_MEM;
            }
        } else {
            return E_NO_MEM;
        }
        va += PAGE_SIZE;
    }
    return 0;
}


// extra function (nouran)
// bahot all free pages ba3d ma bain bulks fe list
// lma bagy adwr btb2a already sorted desc
// kda el worst fit hwa awel element (fast)

void insert_bulk_desc(struct Bulk* b) {
	struct Bulk *cur = bulks;
	struct Bulk *prev = NULL;

	while (cur && cur->size > b->size) {
		prev = cur;
		cur = cur->next;
}

	b->prev = prev;
	b->next = cur;

	if (prev){
		prev->next = b;
	}

	else{
		bulks = b;
	}

	if (cur){
		cur->prev = b;
	}

}

// extra function (nouran)
//void remove_bulk
void remove_bulk(struct Bulk* b) {

	if (b->prev){
		b->prev->next = b->next;
	}
	else{
		bulks = b->next;
	}

	if (b->next){
		b->next->prev = b->prev;
	}

	b->prev = b->next = NULL;
}

//extra function (nouran)
void* page_alloc_custom_fit(uint32 needed_bytes){
	//exact fit
	if(bulks && bulks->size >= needed_bytes){
		for(struct Bulk* b = bulks; b ; b = b->next){
				if(b->size == needed_bytes){
					uint32 va = b->start;

					//hashelha mn el free bulks list
					remove_bulk(b);


					if(map_frame_range(va, needed_bytes) == E_NO_MEM) {
						return NULL;
					}
					struct Bulk* alloc_bulk = (struct Bulk*) alloc_block(sizeof(struct Bulk));
					alloc_bulk->start = va;
					alloc_bulk->size = needed_bytes;
					alloc_bulk->next = bulks_allocated;
					alloc_bulk->prev = NULL;
					if (bulks_allocated) bulks_allocated->prev = alloc_bulk;
					bulks_allocated = alloc_bulk;
					return (void *)va;
				}
			}
	}


	//worst fit
	if (bulks && bulks->size > needed_bytes) {
		struct Bulk* bb = bulks; //awel element dayman akbar wahed
		uint32 va = bb->start;

		// hasheel el adima mn el list
		remove_bulk(bb);

		//elstart hatetharak n bytes
		bb->start += needed_bytes;

		//el size hy2el n bytes
		bb->size -= needed_bytes;

		//ha insert el gedida
		insert_bulk_desc(bb);

		// === FIX: Replace the broken loop with this single call ===
		if(map_frame_range(va, needed_bytes) == E_NO_MEM){
			return (void *)NULL;
		}
		// =========================================================

		struct Bulk* alloc_bulk = (struct Bulk*) alloc_block(sizeof(struct Bulk));
		alloc_bulk->start = va;
		alloc_bulk->size = needed_bytes;
		alloc_bulk->next = bulks_allocated;
		alloc_bulk->prev = NULL;
		if (bulks_allocated) bulks_allocated->prev = alloc_bulk;
		bulks_allocated = alloc_bulk;

		return (void *)va;
	}


	//some validations
	if(kheapPageAllocBreak < kheapPageAllocStart){
		return (void *)NULL;
	}
	if(needed_bytes > KERNEL_HEAP_MAX - kheapPageAllocBreak){
		return (void *)NULL;
	}
	if (kheapPageAllocBreak + needed_bytes > KERNEL_HEAP_MAX) {
		return (void *)NULL;
	}
	uint32 va = kheapPageAllocBreak;


	for(int i = 0 ; i < needed_bytes; i += PAGE_SIZE){
		struct FrameInfo* fi;
		if(allocate_frame(&fi) == E_NO_MEM){
			return (void *)NULL;
		}
		if(map_frame(ptr_page_directory, fi, va + i, PERM_WRITEABLE) == E_NO_MEM){
			free_frame(fi);
			return (void *)NULL;
		}
	}
	struct Bulk* alloc_bulk = (struct Bulk*) alloc_block(sizeof(struct Bulk));
	alloc_bulk->start = va;
	alloc_bulk->size = needed_bytes;
	alloc_bulk->next = bulks_allocated;
	alloc_bulk->prev = NULL;
	if (bulks_allocated) bulks_allocated->prev = alloc_bulk;
	bulks_allocated = alloc_bulk;

	//harfa3 elbreak
	kheapPageAllocBreak += needed_bytes;
	return (void*)va;

}

//helper function for kfree (hamed)
// In kheap.c

void merge_bulks(uint32 start, uint32 size){
	struct Bulk *current = bulks;
	struct Bulk *abl=NULL , *ba3d=NULL;

	while(current){
		if(current->start + current->size == start) abl=current;
		if(current->start == start+size) ba3d=current;
		current=current->next;
	}

	if(abl && ba3d){
		// nesheel el 2 bulks
		remove_bulk(abl);
		remove_bulk(ba3d);

		// merge el size
		abl->size += size + ba3d->size;

		//dakhaal tany
		insert_bulk_desc(abl);

		// Free
		free_block(ba3d);
	}
	else if(abl){
		// Remove the block
		remove_bulk(abl);
		abl->size += size;
		//  Re-insert to re-sort
		insert_bulk_desc(abl);
	}
	else if(ba3d){
		remove_bulk(ba3d);
		ba3d->start=start;
		ba3d->size += size;
//		insert tanhy
		insert_bulk_desc(ba3d);
	}
	else{
		struct Bulk *freebulk = (struct Bulk*) alloc_block(sizeof(struct Bulk));
		if (freebulk == NULL) {
		    panic("Failed to allocate Bulk struct for free region!");
		}
		freebulk->start = start;
		freebulk->size = size;
		freebulk->next = NULL;
		freebulk->prev = NULL;
		insert_bulk_desc(freebulk);
	}
}

void* kmalloc(unsigned int size)
{
	#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	//Your code is here
	//Comment the following line
	//kpanic_into_prompt("kmalloc() is not implemented yet...!!");

	acquire_kspinlock(&kheap_lock);
	//invalid size
	if (size == 0){
		release_kspinlock(&kheap_lock);
		return (void *)NULL;
	}
	void* ptr=NULL;
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE){
		ptr = alloc_block(size);
	}
	else{
		uint32 needed_bytes = ROUNDUP(size , PAGE_SIZE);
		ptr = page_alloc_custom_fit(needed_bytes);
	}
	release_kspinlock(&kheap_lock);
	return ptr;
	//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
	#endif
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	acquire_kspinlock(&kheap_lock);


	if(virtual_address==NULL){
		release_kspinlock(&kheap_lock);
		return;
	}

	if((uint32)virtual_address < KERNEL_HEAP_START || (uint32)virtual_address >= KERNEL_HEAP_MAX){
		panic("address out of bounds");
	}

	//7edod el regions
	uint32 blockstart=KERNEL_HEAP_START;
	uint32 blockend=KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE;
	uint32 pagestart=kheapPageAllocStart;
	uint32 pageend=KERNEL_HEAP_MAX;

	//law gowa el block region
	if((uint32)virtual_address >= blockstart && (uint32)virtual_address < blockend){
		free_block(virtual_address);
		release_kspinlock(&kheap_lock);
		return;
	}

	//law gowa el page region
	if((uint32)virtual_address >= pagestart && (uint32)virtual_address < pageend){
		//finding bulk
		struct Bulk *currentbulk= bulks_allocated;
		while(currentbulk){
			if(currentbulk->start == (uint32)virtual_address) break;
			currentbulk=currentbulk->next;
		}
		if(!currentbulk){
			panic("Pointer not allocated or already freed");
		}

		//unmapping bulk
		for(uint32 va=currentbulk->start; va < currentbulk->start + currentbulk->size; va+=PAGE_SIZE){
			unmap_frame(ptr_page_directory,va);
		}

		//remove bulk from list
		if (currentbulk->prev) currentbulk->prev->next = currentbulk->next;
		if (currentbulk->next) currentbulk->next->prev = currentbulk->prev;
		if (bulks_allocated == currentbulk) bulks_allocated = currentbulk->next;

		//ne5azn el info beta3t el bulk 3ashan el merge
		uint32 freed_start = currentbulk->start;
		uint32 freed_size = currentbulk->size;
		free_block(currentbulk); // na3ml free le el bulk nafso

		//na3ml merge le el bulks be el info el 5azenaha
		merge_bulks(freed_start, freed_size);

		//nenazl el break pointer
		int break_lowered = 1;
		while (break_lowered) {
			break_lowered = 0;
			struct Bulk *cur = bulks;
			struct Bulk *top_free_block = NULL;

			// negeeb a5r block abl el break
			while(cur) {
				if (cur->start + cur->size == kheapPageAllocBreak) {
					top_free_block = cur;
					break;
				}
				cur = cur->next;
			}

			// If we found one, remove it, lower break, and loop again
			if (top_free_block) {
				remove_bulk(top_free_block); // Remove from free list
				kheapPageAllocBreak = top_free_block->start; // Lower the break
				free_block(top_free_block);
				break_lowered = 1;
			}
		}
		release_kspinlock(&kheap_lock);
		return;
	}

#endif
	//panic("kfree: invalid address to free (not in block or page region)");
}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	//Your code is here

	acquire_kspinlock(&kheap_lock);

	//negeb el frame be el physical address
	struct FrameInfo* fi = to_frame_info(physical_address);

	//net2akd eno mapped
	if (fi == NULL || fi->va == 0)
	{
		release_kspinlock(&kheap_lock);
		return 0;
	}

	//negeb el virtual address bas mn el frame
	uint32 va_base = fi->va;

	bool in_block_region = (va_base >= dynAllocStart && va_base < dynAllocEnd);
	bool in_page_region = (va_base >= kheapPageAllocStart && va_base < KERNEL_HEAP_MAX);

	if (!in_block_region && !in_page_region)
	{
		release_kspinlock(&kheap_lock);
		//not available
		return 0;
	}

	//ne3ml el virtual address
	uint32 offset = physical_address % PAGE_SIZE;

	uint32 virtaddr = va_base + offset;
	release_kspinlock(&kheap_lock);
	return virtaddr;
	//Comment the following line
//	panic("kheap_virtual_address() is not implemented yet...!!");

	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
#endif
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	//Your code is here

	/* EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */

	acquire_kspinlock(&kheap_lock);

	bool in_block_region = (virtual_address >= dynAllocStart && virtual_address < dynAllocEnd);
	bool in_page_region = (virtual_address >= kheapPageAllocStart && virtual_address < KERNEL_HEAP_MAX);

	//net2akd eno fe el region
	if (!in_block_region && !in_page_region)
	{
		release_kspinlock(&kheap_lock);
		return 0;
	}

	//negeb el pointer beta3 el table
	uint32* ptr_page_table;
	get_page_table(ptr_page_directory, virtual_address, &ptr_page_table);

	if (ptr_page_table == NULL)
	{
		release_kspinlock(&kheap_lock);
		return 0;
	}

	//negeb el table index we el entry
	uint32 page_table_index = PTX(virtual_address);
	uint32 entry = ptr_page_table[page_table_index];

	//net2akd eno el entry mawgoda fe3ln
	if (!(entry & PERM_PRESENT))
	{
		release_kspinlock(&kheap_lock);
	    return 0;
	}

	//ne3ml mask le bits
	uint32 physical_base = entry & 0xFFFFF000;
	uint32 offset = virtual_address % PAGE_SIZE;
	uint32 physical_address = physical_base + offset;

	release_kspinlock(&kheap_lock);
	return physical_address;
#endif
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
