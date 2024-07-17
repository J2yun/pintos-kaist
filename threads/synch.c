/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
/* 프로젝트 1-2: waiter에 스레드 넣을 때 priority 순서로 들어가도록 */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	/* 프로젝트 1-2: while문을 사용하는 이유 스레드가 unblock 되었을 때, << thread_block() 에서 다시 코드가 진행되기 시작함 >>.
		이때, if문을 사용하게 되면 sema->value가 0이어도 다음 코드로 진행되어 오류 발생, 이를 방지하기 위해 while문을 사용하여
		unblock 되었을 때 다시 세마포어의 상태를 확하는 것 */
	while (sema->value == 0) {
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		list_insert_ordered (&sema->waiters, &thread_current()->elem, thread_priority_cmp, NULL);
		thread_block (); // 현재 스레드를 차단 (이 지점에서 실행이 중단됨)
	}
	sema->value--; // 세마포어 값 감소 (스레드가 깨어난 후 실행됨)
	intr_set_level (old_level);
	
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/* 프로젝트 1-2: waiter 정렬하기 - waiter 리스트에 있는 스레드의 priority가 수정될 수 있기 때문 */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	
	if (!list_empty (&sema->waiters)) { // 중괄호 없던거 ㄹㅈㄷ
		// 여기서 sort 하는 이유, waiter 내의 priority가 변경되어 있을 수도 있다. 
		// thread_set_priority에서는 ready list만 sort 하고 waiters는 sort 안함
		list_sort(&sema->waiters, thread_priority_cmp, NULL);	
		// waiters 에서 pop 하기 전에 정렬이 되어있어야 우선 순위에 맞게 pop 됨
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	}
	sema->value++;
	// 이거 때문에 3시간 삽질함!!!!!!!!!!!!!!!!
	thread_yield(); // Unlock 되고나서 다음 스레드가 점유할 수 있도록 하기 위해 필요
	intr_set_level (old_level);
	
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* 프로젝트 1-2: 락 불가능한 경우 -> 락 세마포어 기다리게 하기, priority donation */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	// struct thread *curr = thread_current();

	if (lock->holder) { // 락이 불가능 한 경우 (lock->holder가 NULL이 아닌 경우)
		// 아래 로직은 lock_try_acquired에서 실행됨
		// list_insert_ordered(&lock->semaphore.waiters, &curr->d_elem, thread_priority_func, NULL);
		thread_current()->wait_on_lock = lock;
		// printf(" donation 이전 holder priority: %d\n\n", lock->holder->priority);
		list_insert_ordered(&lock->holder->donations, &thread_current()->d_elem, thread_priority_cmp, NULL);

		// struct list_elem *e = list_begin(&(lock->holder->dontaions));
		// for (e; e!=list_end(&lock->holder->dontaions); e=list_next(e)) {
		// 	struct thread *t = list_entry(e,struct thread, d_elem);
		// 	printf("%d" ,t->priority);
		// }
		// printf("\n");
			
		// priority donation 로직 수행
		priority_donation();
	} // lock_try_acquire 가 true일 경우 해당 함수 안에서 sema_down이랑 lock->holer 실행됨 
	
	sema_down (&lock->semaphore);
	thread_current()->wait_on_lock = NULL;
	lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;

	remove_with_lock(lock);
	refresh_priority();

	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* 프로젝트 1-2: waiter에 스레드 넣을 때 priority 순서로 들어가도록 */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	// list_push_back (&cond->waiters, &waiter.elem);
	list_insert_ordered(&cond->waiters, &waiter.elem, sema_elem_priority_cmp, NULL);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/* 프로젝트 1-2: waiter 정렬하기 - waiter 리스트에 있는 스레드의 priority가 수정될 수 있기 때문 */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		// waiters 에서 pop 하기 전에 정렬이 되어있어야 우선 순위에 맞게 pop 됨	
		list_sort(&cond->waiters, sema_elem_priority_cmp, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

/* 프로젝트 1-2: Priority Donation 
	현재 스레드가 lock을 획득하지 못했을 때 호출 됨. 위로 올라가면서 donate 함*/
void
priority_donation(void) {
	struct thread *t = thread_current();
	struct lock *lock = t->wait_on_lock;

	while (true) {
		if (t->wait_on_lock == NULL) {
			// nested donation 끝부분일 경우
			break;
		}

		if (t->priority > lock->holder->priority) {
			lock->holder->priority = t->priority;
			t = lock->holder;
			lock = t->wait_on_lock;
		}
	}
}

/* 프로젝트 1-2: 현재 스레드의 priority를 donation list를 보고 재설정 
	unlock 되거나, thread_set_priority 시 호출 됨*/
void 
refresh_priority(void) {
	struct thread *t = thread_current();
	// struct lock *lock = t->wait_on_lock;

	t->priority = t->original_priority; // 우선순위 초기화

	if (!list_empty(&t->donations)) {// donation 할 놈들이 있을 때 호출되어야 함
		// donation list 첫번째(가장 큰 우선순위)가 내 priority 보다 높을 경우 donation
		struct thread *d_thread = list_entry(list_front(&t->donations),struct thread, d_elem);
		if (t->priority < d_thread->priority) {
				t->priority = d_thread->priority;
		}
	}
}

void
remove_with_lock(struct lock *lock) {
	// struct thread *t = thread_current();
	struct semaphore *sema = &lock->semaphore;

	struct list_elem *e;
	for (e = list_begin(&sema->waiters); e != list_end(&sema->waiters); e = list_next(e)) {
		// d_elem은 donations list에만 연결되어있으니 list_remove(d_elem) 형태 사용 가능
		struct thread *t = list_entry(e,struct thread, elem);
		list_remove(&t->d_elem);
	}
}


// /* 프로젝트 1-2: 스레드 우선 순위에 따라 정렬하기 위한 function*/
bool sema_elem_priority_cmp(const struct list_elem *e1, const struct list_elem *e2, void *aux) {
	struct semaphore_elem *s1 = list_entry(e1, struct semaphore_elem, elem);
	struct semaphore_elem *s2 = list_entry(e2, struct semaphore_elem, elem);

	// if (list_empty(&s1->semaphore.waiters)) {
	// 	printf("비어있음\n");
	// 	return false;
	// }

	struct thread *t1 = list_entry(list_begin(&s1->semaphore.waiters), struct thread, elem);
	struct thread *t2 = list_entry(list_begin(&s2->semaphore.waiters), struct thread, elem);

	// printf("fuction check! t1: %d, t2:%d \n",t1->wakeup_tick, t2->wakeup_tick);
	return (t1->priority > t2->priority);
}