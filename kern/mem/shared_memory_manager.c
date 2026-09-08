#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
			//cprintf("shared var name = %s compared with %s\n", name, shr->name);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	return ret;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}
//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here
	//Comment the following line
	//panic("alloc_share() is not implemented yet...!!");

	acquire_kspinlock(&(AllShares.shareslock));
	struct Share* ostazShared = kmalloc(sizeof(struct Share));
	release_kspinlock(&(AllShares.shareslock));


	ostazShared->ownerID = ownerID;
	ostazShared->size = size;
	ostazShared->isWritable = isWritable;
	ostazShared->references = 1;
		strncpy(ostazShared->name, shareName, 64);

		int framesNum = ROUNDUP(size, PAGE_SIZE)/PAGE_SIZE;

		ostazShared->framesStorage = kmalloc(sizeof(struct FrameInfo*) * framesNum);

		if(ostazShared->framesStorage == NULL){

			acquire_kspinlock(&(AllShares.shareslock));
			kfree(ostazShared);
			release_kspinlock(&(AllShares.shareslock));

			return NULL;
		}

		memset(ostazShared->framesStorage, 0 ,sizeof(struct FrameInfo*) * framesNum);

		return ostazShared;
#endif
}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
	//Your code is here
	//Comment the following line
	//panic("create_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	struct Share* ostazSharedMawgood = find_share(ownerID, shareName);

	if(ostazSharedMawgood != NULL){
		return E_SHARED_MEM_EXISTS;
	}

	struct Share* ostazShared = alloc_share(ownerID, shareName, size, isWritable);

	if(ostazShared == NULL){
		return E_NO_SHARE;
	}

	acquire_kspinlock(&(AllShares.shareslock));
	{
		LIST_INSERT_HEAD(&AllShares.shares_list, ostazShared);
	}
	release_kspinlock(&(AllShares.shareslock));

	ostazShared->ID = (uint32)virtual_address & 0x7FFFFFFF;

	uint32 neededPages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	uint32 va = (uint32)virtual_address;

	for(uint32 i = 0 ; i < neededPages ; i++){

		struct FrameInfo* fi;

		if(allocate_frame(&fi) < 0 || map_frame(myenv->env_page_directory, fi, va, PERM_USER | PERM_WRITEABLE) < 0){
			return E_NO_SHARE;
		}

		ostazShared->framesStorage[i] = fi;
		va+=PAGE_SIZE;
	}

	return ostazShared->ID;

	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_EXISTS if the shared object already exists
	//	c) E_NO_SHARE if failed to create a shared object
#endif
}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
	//Your code is here
	//Comment the following line
	//panic("get_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// 	This function should share the required object in the heap of the current environment
	//	starting from the given virtual_address with the specified permissions of the object: read_only/writable
	// 	and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	struct Share* ostazShared = NULL;

	acquire_kspinlock(&(AllShares.shareslock));

	struct Share* shr;
	LIST_FOREACH_SAFE(shr, &AllShares.shares_list,Share){
		if(shr->ownerID == ownerID && strcmp(shr->name, shareName) == 0){
			ostazShared = shr;
			ostazShared->references++;
			break;
		}
	}

	release_kspinlock(&(AllShares.shareslock));

	if(ostazShared == NULL){
		return E_SHARED_MEM_NOT_EXISTS;
	}

	int permission = PERM_USER;

	if(ostazShared->isWritable){
		permission= permission | PERM_WRITEABLE;
	}

	uint32 va = (uint32)virtual_address;
	uint32 neededPages = ROUNDUP(ostazShared->size, PAGE_SIZE) / PAGE_SIZE;

	for(uint32 i = 0 ; i < neededPages ; i++){
		if(map_frame(myenv->env_page_directory, ostazShared->framesStorage[i], va, permission) < 0){
			return E_SHARED_MEM_NOT_EXISTS;
		}
		va += PAGE_SIZE;
	}

	return ostazShared ->ID;
#endif
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	panic("free_share() is not implemented yet...!!");
}


//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	panic("delete_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	//	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"

}
