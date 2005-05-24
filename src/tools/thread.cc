/*
 *	HT Editor
 *	thread.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cstdio>
#include <cstdlib>

#include "debug.h"
#include "except.h"
#include "thread.h"

static void *start_thread_routine(void *t)
{
	Thread *thread = ((Thread *)t);
	thread->setState(threadRunning);
	void *ret = thread->run();
	if (thread->getState() == threadRunning) {
		/*
		 *	Thread reached end of run(), and most likely
		 *	didn't call terminate(), so we do here
		 */
		 thread->terminate(ret);
	}
	return ret;
}

Thread::Thread()
{
	mThreadValid = false;
	mState = threadInited;
	mSem = new Semaphore();
}

Thread::~Thread()
{
	if (mState == threadStarting || mState == threadRunning)  {
		// in a perfect world, we'd throw an exception here
		fprintf(stderr, "Attempt to kill running thread!\n");
		// but world isn't perfect
		exit(1);
	}
	if (mThreadValid) {
		sys_destroy_thread(mThread);
	}
}

void Thread::start(void *param)
{
	setState(threadStarting);
	mThreadValid = true;
	sys_create_thread(&mThread, 0, start_thread_routine, this);
}

ThreadState Thread::getState()
{
	return mState;
}

void Thread::setState(ThreadState state)
{
	mSem->lock();
	mState = state;
	mSem->signalAll();
	mSem->unlock();
}

void *Thread::waitForExit()
{
	mSem->lock();
	while (mState == threadStarting) {
		mSem->wait();
	}
	mSem->unlock();
	mSem->lock();
	while (mState == threadRunning) {
		mSem->wait();
	}
	mSem->unlock();
	return NULL;
}

void Thread::waitForRunning()
{
	mSem->lock();
	while (mState == threadStarting) {
		mSem->wait();
	}
	mSem->unlock();
}

void Thread::terminate(void *ret)
{
	if (mState == threadRunning) {
		ASSERT(mThreadValid);
		setState(threadFinished);
		sys_exit_thread(ret);
	} else {
		throw MsgException("Arrrrrrg. Please read the docu. You musn't call terminate() from outside the thread.");
	}
}

/*
 *
 */
ThreadFunction::ThreadFunction(sys_thread_function f, void *aParam)
	:Thread()
{
	mFunc = f;
	mParam = aParam;
}

void *ThreadFunction::run()
{
	terminate(mFunc(mParam));
	// terminate does not return
	return NULL;
}

/*
 *
 */
Mutex::Mutex()
{
	sys_create_mutex(&mutex);
}

Mutex::~Mutex()
{
	sys_destroy_mutex(mutex);
}

void Mutex::lock()
{
	sys_lock_mutex(mutex);
}

void Mutex::unlock()
{
	sys_unlock_mutex(mutex);
}

int Mutex::trylock()
{
	return sys_trylock_mutex(mutex);
}

/*
 *
 */

Semaphore::Semaphore()
{
	sys_create_semaphore(&sem);
}

Semaphore::~Semaphore()
{
	sys_destroy_semaphore(sem);
}

void Semaphore::signal()
{
	sys_signal_semaphore(sem);
}

void Semaphore::signalAll()
{
	sys_signal_all_semaphore(sem);
}

void Semaphore::wait()
{
	sys_wait_semaphore(sem);
}

void Semaphore::lock()
{
	sys_lock_semaphore(sem);
}

void Semaphore::unlock()
{
	sys_unlock_semaphore(sem);
}

