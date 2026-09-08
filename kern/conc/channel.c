/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct kspinlock* lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #1 CHANNEL - sleep
	//Your code is here

	struct Env* current_env = get_cpu_proc(); // current running process

		if (current_env == NULL) panic("sleep: no process is currently running");
		if (lk == NULL) panic("sleep: no lock provided");

		acquire_kspinlock(&(ProcessQueues.qlock));

		// Release the guard
		release_kspinlock(lk);

		// Update status and enqueue
		current_env->env_status = ENV_BLOCKED;
		current_env->channel = chan;
		enqueue(&(chan->queue), current_env);

		//
		sched();

		//
		release_kspinlock(&(ProcessQueues.qlock));

		// Re-acquire guard
		acquire_kspinlock(lk);

		current_env->channel = NULL;

	//Comment the following line
//	panic("sleep() is not implemented yet...!!");
}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #2 CHANNEL - wakeup_one
	//Your code is here

	if (chan == NULL) return;


		acquire_kspinlock(&(ProcessQueues.qlock));


		struct Env* wk_env = dequeue(&(chan->queue));

		// single condition to wake one
		if (wk_env != NULL)
		{
			wk_env->env_status = ENV_READY;
			wk_env->channel = NULL;

			sched_insert_ready(wk_env);
		}

		release_kspinlock(&(ProcessQueues.qlock));

	//Comment the following line
//	panic("wakeup_one() is not implemented yet...!!");
}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #3 CHANNEL - wakeup_all
	//Your code is here
	if (chan == NULL) return;

		acquire_kspinlock(&(ProcessQueues.qlock));

		struct Env* ptr_env_to_wake;

		// loop cause wake_all
		while ((ptr_env_to_wake = dequeue(&(chan->queue))) != NULL)
		{
			ptr_env_to_wake->env_status = ENV_READY;
			ptr_env_to_wake->channel = NULL;

			sched_insert_ready(ptr_env_to_wake);
		}

		release_kspinlock(&(ProcessQueues.qlock));


	//Comment the following line
//	panic("wakeup_all() is not implemented yet...!!");
}

