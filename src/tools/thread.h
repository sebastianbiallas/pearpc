/*
 *	HT Editor
 *	thread.h
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

#ifndef ___THREAD_H__
#define ___THREAD_H__

#include "system/systhread.h"

enum ThreadState {
threadInited,
threadStarting,
threadRunning,
threadFinished,
};

class Mutex;
class Semaphore;

class Thread {
	sys_thread	mThread;
	bool		mThreadValid;
	ThreadState	mState;
	Semaphore *	mSem;
public:
			Thread();
	virtual		~Thread();
/**
 *	Call this to start the thread.
 *	You can call this again, when the thread has finished,
 *	but note that you will mostlikely get an other thread id.
 */	
		void	start(void *param);

/**
 *
 */
		ThreadState	getState();
		
/*
 *	The following functions can only be called from *other* threads
 *	(I.e. not from run())
 */
 
/**	
 *	Wait until the thread exits.
 *	@returns return value from run() [FIXME: broken]
 */
		void	*waitForExit();
/**
 *	Waits until the thread is running (if thread has been started with start() before)
 */		
		void	waitForRunning();
		
/*protected: (I'd really love to make this protected, but I can't for some stupid reasons) */

/**
 *	The body of the thread, must be implemented.
 *	Don't call this directly but via start().
 */
	virtual	void *	run() = 0;
/**	
 *	Don't call this.
 */
		void	setState(ThreadState ts);
		
/*
 *	These functions can only be called from the this thread
 *	(I.e. from run())
 */
 
/**
 *	Terminate current thread.
 *	Equivalent to return in run()
 */ 
		void 	terminate(void *ret);
};

/**
 *	Use this if you dont want to use Thread.
 *	Just takes a function pointer as argument.
 */
class ThreadFunction: public Thread {
	sys_thread_function mFunc;
	void *mParam;
public:
			ThreadFunction(sys_thread_function f, void *aParam);
	virtual void *	run();
};

class Mutex {
	sys_mutex mutex;
public:
			Mutex();
	virtual		~Mutex();
		void	lock();
		void	unlock();
/**
 *	Tries to lock the mutex.
 *	@returns 0 if it wasn't locked (and is now locked) or EBUSY if it is already locked.
 */		
		int	trylock();
};

class Semaphore {
	sys_semaphore sem;
public:
			Semaphore();
	virtual		~Semaphore();
/**
 *	Signals a random thread which is wait()ing for Semaphore.
 *	You have to call lock() before.
 */
		void	signal();
/**
 *	Signals all threads which are wait()ing for Semaphore.
 *	You have to call lock() before.
 */
		void	signalAll();
		
/**
 *	Locks Mutex for wait()
 */
		void	lock();
		
/**
 *	Wait for Semaphore until some other Thread signal()s it.
 *	You have to call lock() before.
 */
		void	wait();

/**
 *	Locks Mutex for wait()
 */
		void	unlock();
};

#endif
