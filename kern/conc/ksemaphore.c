// Kernel-level Semaphore

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "ksemaphore.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

void init_ksemaphore(struct ksemaphore *ksem, int value, char *name)
{
	init_channel(&(ksem->chan), "ksemaphore channel");
	init_kspinlock(&(ksem->lk), "lock of ksemaphore");
	strcpy(ksem->name, name);
	ksem->count = value;
}

void wait_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #6 SEMAPHORE - wait_ksemaphore
	//Your code is here

	// Acquire the lock to protect the shared count
		acquire_kspinlock(&(ksem->lk));


		//Decrement the semaphore value
		ksem->count--;


		// If negative, we must block
		if (ksem->count < 0)
		{
			// sleep will release the guard and block us and re-acquire guard when we wake
			sleep(&(ksem->chan), &(ksem->lk));
		}


		// Release the lock
		release_kspinlock(&(ksem->lk));


	//Comment the following line
//	panic("wait_ksemaphore() is not implemented yet...!!");

}

void signal_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #7 SEMAPHORE - signal_ksemaphore
	//Your code is here

	//Acquire the lock
		acquire_kspinlock(&(ksem->lk));

		// Increment the semaphore value
		ksem->count++;

		// If count is <= 0, it means someone is sleeping and needs to be woken
		if (ksem->count <= 0)
		{
			wakeup_one(&(ksem->chan));
		}

		// Release the lock
		release_kspinlock(&(ksem->lk));

	//Comment the following line
//	panic("signal_ksemaphore() is not implemented yet...!!");

}


