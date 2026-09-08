/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}
/*2024*/ void setFASTNchanceCLOCK(bool fast){ FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL(){ _PageRepAlgoType = PG_REP_OPTIMAL; };

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL(){if(_PageRepAlgoType == PG_REP_OPTIMAL) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0) ;
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;
extern uint32 sys_calculate_free_frames() ;

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter ++ ;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
#if USE_KHEAP
			   int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);

				// 1 UNMARKED page in user heap
				if (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) {
					if ((perms & PERM_UHPAGE) == 0) {
						env_exit();
						return;
					}
				}
				//

				// 2 trying to access kernel
				if (fault_va >= USTACKTOP) {
					env_exit();
					return;
				}
				//

				// 3 existing page with read only
				if ((perms & PERM_PRESENT)  && !(perms & PERM_WRITEABLE)) {
					env_exit();
					return;
				}
#endif
			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}


//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */
int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize, struct PageRef_List *pageReferences)
{
	//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
	//Your code is here
#if USE_KHEAP
	uint32 *working_set_beta3ty = (uint32 *)kmalloc(maxWSSize * sizeof(uint32));
	int current_size=0;
	int faults=0;

	//nen2l le 7aga le el working set el 3amltha
	struct WorkingSetElement *iterator;
	LIST_FOREACH_SAFE(iterator, initWorkingSet, WorkingSetElement)
	{
		if (current_size < maxWSSize)
		{
			working_set_beta3ty[current_size++] = iterator->virtual_address;
		}
	}

	//nemshe 3ala el reference stream
	struct PageRefElement *main_iterator;
	{
		LIST_FOREACH_SAFE(main_iterator, pageReferences, PageRefElement){
			uint32 current_page = ROUNDDOWN(main_iterator->virtual_address, PAGE_SIZE);
			int mawgood=0;

			//net2akd law el page fe el RAM
			for(int i=0; i<current_size; i++){
				if(working_set_beta3ty[i] == current_page){
					mawgood=1;
					break;
				}
			}

			if(mawgood)continue; //Hit

			faults++; //Miss

			if(current_size < maxWSSize){
				//na3ml placement
				working_set_beta3ty[current_size++] = current_page;
			}
			else{
				//replacement
				int victim_index=0;
				int ab3ad_masafa=0;

				for(int i=0;i<current_size;i++){
					int distance=0;
					int mawgood_2odam=0;

					struct PageRefElement *iterator3 = LIST_NEXT(main_iterator);
					//neshof el stream
					while(iterator3 != NULL){
						distance++;
						if (ROUNDDOWN(iterator3->virtual_address, PAGE_SIZE) == working_set_beta3ty[i])
						{
							mawgood_2odam = 1;
							break;
						}
						iterator3 = LIST_NEXT(iterator3);
					}
					if(!mawgood_2odam){
						distance= 2147483647; //akbar integer
					}
					if(distance > ab3ad_masafa){
						ab3ad_masafa = distance;
						victim_index = i;
					}
					if(ab3ad_masafa == 2147483647) break; //mesh mawgood 2odam
				}
				//na3ml replace le el victim
				working_set_beta3ty[victim_index]=current_page;
			}
		}
	}
	kfree(working_set_beta3ty);
	return faults;
#endif
}


void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
	{
		//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		//Your code is here

		struct PageRefElement *ref_geded = kmalloc(sizeof(struct PageRefElement));
		ref_geded->virtual_address = ROUNDDOWN(fault_va,PAGE_SIZE);
		LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), ref_geded);

		//na3ml el hook 3ashan nesagl el references
		if(LIST_EMPTY(&(faulted_env->ActiveList)) && faulted_env->page_WS_max_size<1000){ //1000 3ashan el stress tests
			struct WorkingSetElement *iterator; //3ashan el loop
			LIST_FOREACH_SAFE(iterator, &(faulted_env->page_WS_list), WorkingSetElement){
				pt_set_page_permissions(faulted_env->env_page_directory, iterator->virtual_address, 0, PERM_PRESENT);
			}
		}

		//net2akd eno el page fe el memory
		uint32 *table_pointer = NULL;
		struct FrameInfo *frame_info = get_frame_info(faulted_env->env_page_directory, fault_va, &table_pointer);
		if(frame_info == NULL){
			//el page mesh fe el memory fa na3mlha load
			if(allocate_frame(&frame_info) != 0) return;//3ashan law mafesh memory
			map_frame(faulted_env->env_page_directory, frame_info, fault_va, PERM_USER | PERM_WRITEABLE | PERM_PRESENT);

			int ret = pf_read_env_page(faulted_env, (void *)fault_va);
			if (ret == E_PAGE_NOT_EXIST_IN_PF)
			{
				//net2akd eno e7na fe 7edod el stack we el heap
				if (!((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va >= USTACKBOTTOM && fault_va < USTACKTOP)))
				{
					env_exit();
					return;
				}
			}
		}

		struct WorkingSetElement *iterator2 = NULL;
		int mawgood=0;
		LIST_FOREACH_SAFE(iterator2, &(faulted_env->ActiveList), WorkingSetElement){
			if(ROUNDDOWN(iterator2->virtual_address,PAGE_SIZE) == ROUNDDOWN(fault_va, PAGE_SIZE)){
				mawgood=1;
				break;
			}
		}
		if(mawgood){
			//na3ml restore le el present bit
			pt_set_page_permissions(faulted_env->env_page_directory, fault_va, PERM_PRESENT, 0);
		}
		else{//3aml miss

			//law mafesh makan fe el working set
			if(LIST_SIZE(&(faulted_env->ActiveList)) >= faulted_env->page_WS_max_size){
				//na3ml reset le el bits we clear
				struct WorkingSetElement *karar_ezala;
				LIST_FOREACH_SAFE(karar_ezala, &(faulted_env->ActiveList), WorkingSetElement){
					pt_set_page_permissions(faulted_env->env_page_directory, karar_ezala->virtual_address, 0, PERM_PRESENT);
					LIST_REMOVE(&(faulted_env->ActiveList), karar_ezala);
					kfree(karar_ezala);
				}
			}

			//law fe makan fa na3ml add
			struct WorkingSetElement *element_geded = env_page_ws_list_create_element(faulted_env, fault_va);
			LIST_INSERT_TAIL(&(faulted_env->ActiveList), element_geded);
			pt_set_page_permissions(faulted_env->env_page_directory, fault_va, PERM_PRESENT, 0);
		}

		//Comment the following line
		//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
	}
	else
	{
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if(wsSize < (faulted_env->page_WS_max_size))
		{
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
			//Your code is here

			//align el virtual address
			uint32 aligned_va = ROUNDDOWN(fault_va,PAGE_SIZE);
			struct FrameInfo *frame_pointer=NULL;

			//na3ml allocate le el frame
			allocate_frame(&frame_pointer);

			//map el frame
			map_frame(faulted_env->env_page_directory, frame_pointer, aligned_va, PERM_PRESENT | PERM_WRITEABLE | PERM_USER | PERM_USED);

			//na3ml load le el page data
			int read_faulted_data=pf_read_env_page(faulted_env, (void*)aligned_va);
			if(read_faulted_data==E_PAGE_NOT_EXIST_IN_PF){
				//net2akd eno el page mesh fe el kernel
				if (!((aligned_va >= USER_HEAP_START && aligned_va < USER_HEAP_MAX) ||  (aligned_va >= USTACKBOTTOM && aligned_va < USTACKTOP)))
				{
					env_exit();
					return;
				}
			}

			//ne-create el working set
			struct WorkingSetElement *workingset = env_page_ws_list_create_element(faulted_env, aligned_va);
			if(faulted_env->page_last_WS_element == NULL){
				//list mesh full lesa fa na3ml insert 3ady
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list), workingset);
			}
			else{
				//list kanet full
				LIST_INSERT_BEFORE(&(faulted_env->page_WS_list), faulted_env->page_last_WS_element, workingset);
			}

			//ne-update el clock hand
			if(LIST_SIZE(&(faulted_env->page_WS_list)) == faulted_env->page_WS_max_size){
				if(faulted_env->page_last_WS_element == NULL){
					faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
				}
			}

			//Comment the following line
			//panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
		}
		else
		{
			if (isPageReplacmentAlgorithmCLOCK())
			{
				//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
				//Your code is here

				struct WorkingSetElement *victim_element = NULL;

				//nela2e el victim element
				while(1){
					victim_element=faulted_env->page_last_WS_element;
					//get permissions
					uint32 permessions= pt_get_page_permissions(faulted_env->env_page_directory, victim_element->virtual_address);

					if((permessions & PERM_USED) == 0){
						break; //found victim
					}
					else{
						//set bits
						pt_set_page_permissions(faulted_env->env_page_directory,victim_element->virtual_address, 0, PERM_USED);
					}

					//ne7ark el clock hand
					faulted_env->page_last_WS_element = LIST_NEXT(victim_element);

					//law wesl le el a5r neraga3o fo2
					if(faulted_env->page_last_WS_element == NULL){
						faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
					}
				}

				//negeb el info beta3t el victim
				uint32* ptr_page_table=NULL;
				struct FrameInfo* victim_element_frame_info = get_frame_info(faulted_env->env_page_directory, victim_element->virtual_address, &ptr_page_table);

				//neshof law modified
				if (pt_get_page_permissions(faulted_env->env_page_directory, victim_element->virtual_address) & PERM_MODIFIED){
					pf_update_env_page(faulted_env, victim_element->virtual_address, victim_element_frame_info);
				}

				//unmap el victim
				unmap_frame(faulted_env->env_page_directory, (uint32)victim_element->virtual_address);

				//allocate frame geded le el faulted page
				struct FrameInfo* new_frame_info = NULL;
				if (allocate_frame(&new_frame_info) != 0){ //net2akd eno fe memory
					panic("Out of memory");
				}

				//na3ml round down le el address
				uint32 rounded_va = ROUNDDOWN(fault_va,PAGE_SIZE);

				//map el new frame
				map_frame(faulted_env->env_page_directory, new_frame_info, rounded_va, PERM_PRESENT | PERM_WRITEABLE | PERM_USER | PERM_USED);

				//read el page content
				int read_new_page =  pf_read_env_page(faulted_env, (void*)rounded_va);

				if (read_new_page == E_PAGE_NOT_EXIST_IN_PF){
					if (!((rounded_va >= USER_HEAP_START && rounded_va < USER_HEAP_MAX) || (rounded_va >= USTACKBOTTOM && rounded_va < USTACKTOP)))
					{
						env_exit();
						return;
					}
				}

				//na3ml update le el working set
				victim_element->virtual_address = rounded_va;

				//nezawed el clock hand
				faulted_env->page_last_WS_element = LIST_NEXT(victim_element);
				if (faulted_env->page_last_WS_element == NULL){
					faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
				}

				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
			else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				//Your code is here

				struct WorkingSetElement *victim_element = NULL;
				struct WorkingSetElement *current_element = NULL;
				uint32 min_time_stamp = 0xFFFFFFFF; // max value to start at the beginning

				// as8ar time stamp
				LIST_FOREACH(current_element, &(faulted_env->page_WS_list))
				{
					if (current_element->time_stamp < min_time_stamp)
					{
						min_time_stamp = current_element->time_stamp;
						victim_element = current_element;
					}
				}

				if (victim_element == NULL)
				{
					panic(" No victim found in working set, for LRU.");
				}

				// Get victim frame info
				uint32* ptr_page_table = NULL;
				struct FrameInfo* victim_frame_info = get_frame_info(faulted_env->env_page_directory,victim_element->virtual_address,&ptr_page_table);

				if (victim_frame_info == NULL)
				{
					panic("Victim frame info not found, for LRU.");
				}

				// Check if victim page is modified
				uint32 victim_perms = pt_get_page_permissions(faulted_env->env_page_directory,victim_element->virtual_address);
				if (victim_perms & PERM_MODIFIED)
				{
					// Write modified page back to page file
					pf_update_env_page(faulted_env, victim_element->virtual_address, victim_frame_info);
				}

				// Unmap victim frame
				unmap_frame(faulted_env->env_page_directory, victim_element->virtual_address);

				// Allocate new frame for faulted page
				struct FrameInfo* new_frame_info = NULL;
				if (allocate_frame(&new_frame_info) != 0)
				{
					panic("LRU: Out of memory!");
				}

				uint32 rounded_va = ROUNDDOWN(fault_va, PAGE_SIZE);

				// new frame
				map_frame(faulted_env->env_page_directory, new_frame_info, rounded_va,PERM_PRESENT | PERM_WRITEABLE | PERM_USER | PERM_USED);

				// Read page content from page file
				int read_result = pf_read_env_page(faulted_env, (void*)rounded_va);
				if (read_result == E_PAGE_NOT_EXIST_IN_PF)
				{
					// Check if this is a valid heap or stack access
					if (!((rounded_va >= USER_HEAP_START && rounded_va < USER_HEAP_MAX) ||
						  (rounded_va >= USTACKBOTTOM && rounded_va < USTACKTOP)))
					{
						env_exit();
						return;
					}
				}

				// Update victim element with new page info
				victim_element->virtual_address = rounded_va;
				victim_element->time_stamp = (1 << 31); // 0

				LIST_REMOVE(&(faulted_env->page_WS_list), victim_element);
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list), victim_element);

				// Update the pointer to the last element
				if (LIST_SIZE(&(faulted_env->page_WS_list)) == faulted_env->page_WS_max_size) {
				    faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
				} else {
				    faulted_env->page_last_WS_element = NULL;
				}

				pt_set_page_permissions(faulted_env->env_page_directory, rounded_va,PERM_USED, 0);

				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
			else if (isPageReplacmentAlgorithmModifiedCLOCK())
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
				//Your code is here
				struct WorkingSetElement *victim_element = NULL;
				struct WorkingSetElement *current = faulted_env->page_last_WS_element;

				if (current == NULL) {
					current = LIST_FIRST(&(faulted_env->page_WS_list));
				}
				if (current == NULL) {
					panic("Modified Clock: Empty working set during replacement!");
				}

				int found_victim = 0;
				struct WorkingSetElement *start = current;

				//(0,0) not used, not modified
				do {
					uint32 perms = pt_get_page_permissions(faulted_env->env_page_directory,current->virtual_address);
					int used = (perms & PERM_USED) ? 1 : 0;
					int modified = (perms & PERM_MODIFIED) ? 1 : 0;

				if ((perms & PERM_USED) == 0 && (perms & PERM_MODIFIED) == 0) {
						victim_element = current;
						found_victim = 1;
						break;
					}

					current = LIST_NEXT(current);
					if (current == NULL) {
						current = LIST_FIRST(&(faulted_env->page_WS_list));
					}

				} while (current != start && !found_victim);

				//(0,1) not used, but modified
				if (!found_victim) {
					current = start;
					do {
						uint32 perms = pt_get_page_permissions(faulted_env->env_page_directory,current->virtual_address);
						if ((perms & PERM_USED) == 0) {
							victim_element = current;
							found_victim = 1;
							break;
						}

						if (perms & PERM_USED) {
							pt_set_page_permissions(faulted_env->env_page_directory, current->virtual_address, 0, PERM_USED);
						}

						current = LIST_NEXT(current);
						if (current == NULL) {
							current = LIST_FIRST(&(faulted_env->page_WS_list));
						}

					} while (current != start && !found_victim);
				}

				// look for (0,0) again
				if (!found_victim) {
					current = start;
					do {
						uint32 perms = pt_get_page_permissions(faulted_env->env_page_directory,current->virtual_address);
						if ((perms & PERM_USED) == 0 && (perms & PERM_MODIFIED) == 0) {
							victim_element = current;
							found_victim = 1;
							break;
						}

						current = LIST_NEXT(current);
						if (current == NULL) {
							current = LIST_FIRST(&(faulted_env->page_WS_list));
						}

					} while (current != start && !found_victim);
				}

				// current one
				if (!found_victim) {
					victim_element = current;
					found_victim = 1;
				}

				if (victim_element == NULL) {
					victim_element = LIST_FIRST(&(faulted_env->page_WS_list));
				}

				uint32* ptr_page_table = NULL;
				struct FrameInfo* victim_frame_info = get_frame_info(faulted_env->env_page_directory,victim_element->virtual_address,&ptr_page_table);

				if (victim_frame_info == NULL) {
					panic("Victim frame info not found, in modified Clock.");
				}

				uint32 victim_perms = pt_get_page_permissions(faulted_env->env_page_directory,victim_element->virtual_address);
				if (victim_perms & PERM_MODIFIED) {
					pf_update_env_page(faulted_env, victim_element->virtual_address, victim_frame_info);
				}

				unmap_frame(faulted_env->env_page_directory, victim_element->virtual_address);

				struct FrameInfo* new_frame_info = NULL;
				if (allocate_frame(&new_frame_info) != 0) {
					panic("Out of memory, in modified clock");
				}

				uint32 rounded_va = ROUNDDOWN(fault_va, PAGE_SIZE);

				map_frame(faulted_env->env_page_directory, new_frame_info, rounded_va,PERM_PRESENT | PERM_WRITEABLE | PERM_USER | PERM_USED);

				int read_result = pf_read_env_page(faulted_env, (void*)rounded_va);
				if (read_result == E_PAGE_NOT_EXIST_IN_PF) {
					if (!((rounded_va >= USER_HEAP_START && rounded_va < USER_HEAP_MAX) ||
						  (rounded_va >= USTACKBOTTOM && rounded_va < USTACKTOP))) {
						env_exit();
						return;
					}
				}

				victim_element->virtual_address = rounded_va;

				faulted_env->page_last_WS_element = LIST_NEXT(victim_element);
				if (faulted_env->page_last_WS_element == NULL) {
					faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
				}

				pt_set_page_permissions(faulted_env->env_page_directory, rounded_va, PERM_USED, 0);
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
			}
		}
	}
#endif
}


void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}



