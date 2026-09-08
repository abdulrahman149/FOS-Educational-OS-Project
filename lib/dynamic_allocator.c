/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
	//==================================================================================//
	   //============================ HOSSAM FUNCTION 1  ==================================//
	   //==================================================================================//



	   //init the range of the memory " start -> end "
	   dynAllocStart = daStart;
	   dynAllocEnd = daEnd;

	   /* ---------------------------------- space --------------------------------- */
	   /* 'freeBlockList' is an array of linked list so each index must be initialized as a
	     normal linkedList 'head & tail ==>> NULL' so we should loop on all the indices and initialize them
	     using the appendices helper list init function  */
	   // init the freeBlockLists "array of linkedLists"
	   for(uint32  i = 0; i < (LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1); i++){
	       LIST_INIT(&freeBlockLists[i]);
	   }
	   LIST_INIT(&freePagesList);


	   /* ---------------------------------- space --------------------------------- */
	   /*init pageBlockInfoArr array carries number of pages in the dynamic Allocator and each index carries
	    2 thing size of each block inside the page and how many free blocks are left */

	   uint32 total_pages = (daEnd-daStart)/PAGE_SIZE ; //get total number of pages to know the end of the loop
	   for (uint32  i = 0; i < total_pages ; i++){
	       pageBlockInfoArr[i].block_size = 0;
	       pageBlockInfoArr[i].num_of_free_blocks = 0;
	       pageBlockInfoArr[i].prev_next_info.le_next = NULL;
	       pageBlockInfoArr[i].prev_next_info.le_prev = NULL;
	//after init insert the pageBlockInfoArr into the free pagesList by using the helper list insertion function
	       LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[i]);

	   }


//Comment the following line
//	panic("initialize_dynamic_allocator() Not implemented yet");

}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	//==================================================================================//
	//============================ HOSSAM FUNCTION 2  ==================================//
	//==================================================================================//
	/*To get the block size to the corresponding VIRTUAL ADDRESS we need to get an index to access the block size in
	 the pageBlockInfoArr  */
	//check if the VA is within the range os the page
	uint32 offset , index ;
	if ((uint32)va < dynAllocStart || (uint32)va >= dynAllocEnd) {
	    return 0;
	} else {
	    offset = (uint32)va - dynAllocStart;
	    index = offset / PAGE_SIZE;
	    return pageBlockInfoArr[index].block_size;
	}
	//Comment the following line
//	panic("get_block_size() Not implemented yet");
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here

	if (size == 0)
	        return NULL;

	    int log2 = LOG2_MIN_SIZE;
	    uint32 block_size = 1 << log2;
	    while (block_size < size && log2 <= LOG2_MAX_SIZE) {
	        log2++;
	        block_size = 1 << log2;
	    }
	    if (log2 > LOG2_MAX_SIZE)
	        log2 = LOG2_MAX_SIZE;
	    int idx = log2 - LOG2_MIN_SIZE;

	    struct BlockElement *blk = LIST_FIRST(&freeBlockLists[idx]);
	    if (blk != NULL) {
	        LIST_REMOVE(&freeBlockLists[idx], blk);
	        int page_idx = ((uint32)blk - dynAllocStart) / PAGE_SIZE;
	        pageBlockInfoArr[page_idx].num_of_free_blocks--;
	        return (void *)blk;
	    }

	    if (!LIST_EMPTY(&freePagesList)) {
	        struct PageInfoElement *pagehead = LIST_FIRST(&freePagesList);
	        LIST_REMOVE(&freePagesList, pagehead);

	        uint32 page_va = to_page_va(pagehead);

	        get_page((void *)page_va);

	        pagehead->block_size = block_size;
	        pagehead->num_of_free_blocks = PAGE_SIZE / block_size;
	        for (uint32 i = 0; i < pagehead->num_of_free_blocks; i++) {
	            struct BlockElement *b = (struct BlockElement *)(page_va + i * block_size);
	            LIST_INSERT_TAIL(&freeBlockLists[idx], b);
	        }

	        struct BlockElement *alloc_blk = LIST_FIRST(&freeBlockLists[idx]);
	        LIST_REMOVE(&freeBlockLists[idx], alloc_blk);
	        pagehead->num_of_free_blocks--;

	        memset(alloc_blk, 0, block_size);

	        return (void *)alloc_blk;
	    }

	    for (int i = idx + 1; i <= LOG2_MAX_SIZE - LOG2_MIN_SIZE; i++) {
	        struct BlockElement *big_blk = LIST_FIRST(&freeBlockLists[i]);
	        if (big_blk != NULL) {
	            LIST_REMOVE(&freeBlockLists[i], big_blk);
	            int pageindex = ((uint32)big_blk - dynAllocStart) / PAGE_SIZE;
	            pageBlockInfoArr[pageindex].num_of_free_blocks--;
	            return (void *)big_blk;
	        }
	    }

	    panic("alloc_block() is out of free pages and blocks");
	    return NULL;

	//Comment the following line
//	panic("alloc_block() Not implemented yet");

	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	uint32 page_idx = ((uint32)va - dynAllocStart) / PAGE_SIZE;
	struct PageInfoElement *page = &pageBlockInfoArr[page_idx];
	uint32 block_size = page->block_size;

	int log2 = 0, tmp = block_size;
	while (tmp >>= 1) log2++;
	int idx = log2 - LOG2_MIN_SIZE;

	struct BlockElement *blk = (struct BlockElement *)va;
	LIST_INSERT_HEAD(&freeBlockLists[idx], blk);
	page->num_of_free_blocks++;

	uint32 blocks_per_page = PAGE_SIZE / block_size;
	if (page->num_of_free_blocks == blocks_per_page)
	{
		uint32 page_va = to_page_va(page);

		uint32 curr = page_va;
		for (uint32 i = 0; i < blocks_per_page; i++) {
			struct BlockElement *b = (struct BlockElement *)curr;
			LIST_REMOVE(&freeBlockLists[idx], b);
			curr += block_size;
		}

		return_page((void *)page_va);

		page->block_size = 0;
		page->num_of_free_blocks = 0;

		//  Add page back to freePagesList
		LIST_INSERT_TAIL(&freePagesList, page);
	}
	//Comment the following line
	//panic("free_block() Not implemented yet");
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
