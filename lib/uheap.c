#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//initializations for the MALLOC HOSSAM
#define USER_HEAP_ARRAY_SIZE ((USER_HEAP_MAX - USER_HEAP_START) / PAGE_SIZE)
uint32 allocation_sizes[USER_HEAP_ARRAY_SIZE];
// kol el pages elle wara ba3d , ashan lma n allocate w n free
struct Bulk {
	uint32 start;
	uint32 size;
	struct Bulk* next;
	struct Bulk* prev;
};

static struct Bulk* bulks_allocated=NULL;
static struct Bulk* bulks = NULL; // pointer for first element in list
//helpers
void insert_bulk_desc(struct Bulk* b);
void remove_bulk(struct Bulk* b);
void remove_allocated_bulk(struct Bulk *b);
struct Bulk* find_allocated_bulk(uint32 va);
void* user_page_alloc_custom_fit(uint32 needed_bytes);




//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================


void* malloc(uint32 size)
{
#if USE_KHEAP
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
	if( size <= DYN_ALLOC_MAX_BLOCK_SIZE){
		return alloc_block(size);
	}else{
		//allocator using custom fit
		uint32 needed_bytes = ROUNDUP(size , PAGE_SIZE);
		return user_page_alloc_custom_fit(needed_bytes);
	}
	//Comment the following line
//	panic("malloc() is not implemented yet...!!");
#endif

}

//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
/* -------------------------------------------------------------------------- */
/*                     	start of FREE() function by HOSSAM                    */
/* -------------------------------------------------------------------------- */

 //betraga3 el section ===>>  start address == va.
  struct Bulk* find_allocated_bulk(uint32 va)
{
     struct Bulk *cur = bulks_allocated;

     while (cur != NULL)
    {
         if (cur->start == va)
        {
            return cur;
        }
         cur = cur->next;
    }

    return NULL;
}
  //zai el remove bulk bta3et nourana bas bne3mel update fel bulks_allocated
   void remove_allocated_bulk(struct Bulk *b)
{
    if (b == NULL)
        return;

     if (b->prev != NULL)
    {
        b->prev->next = b->next;
    }
    else
    {
         bulks_allocated = b->next;
    }
     if (b->next != NULL)
    {
        b->next->prev = b->prev;
    }
     b->prev = b->next = NULL;
}
  void merge_bulks(uint32 start, uint32 size){
//	  nafs logic el merge el fel kheap bas mngher allocationss 3ashan el panic
    	struct Bulk *current = bulks;
    	struct Bulk *before=NULL , *after=NULL;

    	while(current){
    		if(current->start + current->size == start) before=current;
    		if(current->start == start+size) after=current;
    		current=current->next;
    	}

    	if(before && after){
     		remove_bulk(before);
    		remove_bulk(after);

     		before->size += size + after->size;

     		insert_bulk_desc(before);


    	}
    	else if(before){
     		remove_bulk(before);
     		before->size += size;
     		insert_bulk_desc(before);
    	}
    	else if(after){
     		remove_bulk(after);
     		after->start=start;
    		after->size += size;
     		insert_bulk_desc(after);
    	}
    	else{
     		static struct Bulk free_nodes[2048];
    		static int free_nodes_index = 0;

    		if (free_nodes_index >= 2048)
    			panic("Out of free Bulk nodes!");

    		struct Bulk *freebulk = &free_nodes[free_nodes_index++];

    		freebulk->start = start;
    		freebulk->size = size;
    		freebulk->next = NULL;
    		freebulk->prev = NULL;
    		insert_bulk_desc(freebulk);
    	}
  }

/* -------------------------------------------------------------------------- */
/*                                free function                               */
/* -------------------------------------------------------------------------- */


  void free(void *virtual_address)
  {
#if USE_KHEAP
      if (virtual_address == NULL)
          return;

      uint32 va = (uint32) virtual_address;

       if (va < USER_HEAP_START || va >= USER_HEAP_MAX)
          return;

      //   Dynamic allocator
      if (va >= USER_HEAP_START && va < uheapPageAllocStart)
      {
          free_block(virtual_address);
          return;
      }

      //  PAGE allocator
      uint32 index = (va - USER_HEAP_START) / PAGE_SIZE;
      uint32 size  = allocation_sizes[index];


      if (size == 0)
          return;

       allocation_sizes[index] = 0;
      merge_bulks(va,size);

      // ===== tazbeet el break =====
       uint32 newBrk = uheapPageAllocStart;

       int maxIdx = (uheapPageAllocBreak - USER_HEAP_START) / PAGE_SIZE;

       //scan mn foo2 le ta7t
       for (int i = maxIdx - 1; i >= 0; i--)
      {
          if (allocation_sizes[i] != 0)
          {
               uint32 allVA   = USER_HEAP_START + i * PAGE_SIZE;
              uint32 allSze = allocation_sizes[i];

              newBrk = allVA + allSze;
              break;
          }
      }

      uheapPageAllocBreak = newBrk;
      sys_free_user_mem(va, size);
#endif

  }

//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
#if USE_KHEAP
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	//Comment the following line
	//panic("smalloc() is not implemented yet...!!");
	//struct Env* currentEnv = get_cpu_proc();
	if(sys_size_of_shared_object(sys_getenvid(),sharedVarName) != E_SHARED_MEM_NOT_EXISTS){
		return NULL;
	}

	void* va = user_page_alloc_custom_fit(ROUNDUP(size, PAGE_SIZE));
	if( va == NULL){
		return NULL;
	}

	int objID = sys_create_shared_object(/*sys_getenvid(),*/sharedVarName, size, isWritable, (void*)va);

	if(objID < 0){
		sys_free_user_mem((uint32)va, ROUNDUP(size,PAGE_SIZE));
		return NULL;
	}

	return (void*)va;
#endif
}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
#if USE_KHEAP
   //==============================================================
   //DON'T CHANGE THIS CODE========================================
   uheap_init();
   //==============================================================

   //TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
   //Your code is here
   //Comment the following line
   //panic("sget() is not implemented yet...!!");

   int size = sys_size_of_shared_object(ownerEnvID, sharedVarName);
   if(size<=0){
       return NULL;
   }

   void* va = user_page_alloc_custom_fit(ROUNDUP(size, PAGE_SIZE));
   if(!va){
       return NULL;
   }

   if(sys_get_shared_object(ownerEnvID, sharedVarName,va) < 0){
       sys_free_user_mem((uint32)va, ROUNDUP(size, PAGE_SIZE));
       return NULL;
   }

//    struct Share* sharedObj = sys_size_of_shared_object(ownerEnvID, sharedVarName);
//
//    if(!sharedObj){
//        return (void*)E_SHARED_MEM_NOT_EXISTS;
//    }
//


   return va;

#endif
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//

//malloc helper functions by "NOURAN "
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
void* user_page_alloc_custom_fit(uint32 needed_bytes)
{
    // exact fit
    for(struct Bulk* b = bulks; b ; b = b->next)
    {
        if(b->size == needed_bytes)
        {
            uint32 va = b->start;

            remove_bulk(b);

            sys_allocate_user_mem(va, needed_bytes);

            // Store size in allocation_sizes array
            uint32 index = (va - USER_HEAP_START) / PAGE_SIZE;
            allocation_sizes[index] = needed_bytes;

            return (void*)va;
        }
    }

    // worst fit
    if (bulks && bulks->size > needed_bytes)
    {
        struct Bulk* bb = bulks;
        uint32 va = bb->start;

        remove_bulk(bb);

        bb->start += needed_bytes;
        bb->size  -= needed_bytes;

        insert_bulk_desc(bb);

        sys_allocate_user_mem(va, needed_bytes);

        // Store size in allocation_sizes array
        uint32 index = (va - USER_HEAP_START) / PAGE_SIZE;
        allocation_sizes[index] = needed_bytes;

        return (void*)va;
    }

    // extend the break
    if (uheapPageAllocBreak < uheapPageAllocStart) return NULL;
    if (needed_bytes > USER_HEAP_MAX - uheapPageAllocBreak) return NULL;
    if (uheapPageAllocBreak + needed_bytes > USER_HEAP_MAX) return NULL;

    uint32 va = uheapPageAllocBreak;

    sys_allocate_user_mem(va, needed_bytes);

    // Store size in allocation_sizes array
    uint32 index = (va - USER_HEAP_START) / PAGE_SIZE;
    allocation_sizes[index] = needed_bytes;

    uheapPageAllocBreak += needed_bytes;

    return (void*)va;
}
