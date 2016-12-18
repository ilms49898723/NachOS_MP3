// scheduler.cc
//  Routines to choose the next thread to run, and to dispatch to
//  that thread.
//
//  These routines assume that interrupts are already disabled.
//  If interrupts are disabled, we can assume mutual exclusion
//  (since we are on a uniprocessor).
//
//  NOTE: We can't use Locks to provide mutual exclusion here, since
//  if we needed to wait for a lock, and the lock was busy, we would
//  end up calling FindNextToRun(), and that would put us in an
//  infinite loop.
//
//  Very simple implementation -- no priorities, straight FIFO.
//  Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
//  Initialize the list of ready but not running threads.
//  Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
    // readyList = new List<Thread*>;
    for (int i = 0; i < 4; ++i) {
        L[i] = new List<Thread*>();
    }

    toBeDestroyed = NULL;
    dirty = FALSE;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
//  De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler() {
    // delete readyList;
    for (int i = 0; i < 4; ++i) {
        delete L[i];
    }
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
//  Mark a thread as ready, but not running.
//  Put it on the ready list, for later scheduling onto the CPU.
//
//  "thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread* thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    //cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    // readyList->Append(thread);

    if (kernel->dumpLogToFile) {
        kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID()
                         << " is inserting into queue L" << 3 - thread->getPriority() / 50 << endl;
    } else {
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID()
             << " is inserting into queue L" << 3 - thread->getPriority() / 50 << endl;
    }

    if (thread->getPriority() >= 100) {
        L[1]->Append(thread);
    } else if (thread->getPriority() >= 50) {
        L[2]->Append(thread);
    } else {
        L[3]->Append(thread);
    }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
//  Return the next thread to be scheduled onto the CPU.
//  If there are no ready threads, return NULL.
// Side effect:
//  Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread*
Scheduler::FindNextToRun () {
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    return findNext();
    // if (readyList->IsEmpty()) {
    //     return NULL;
    // } else {
    //     return readyList->RemoveFront();
    // }
}

//----------------------------------------------------------------------
// Scheduler::Run
//  Dispatch the CPU to nextThread.  Save the state of the old thread,
//  and load the state of the new thread, by calling the machine
//  dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//  already been changed from running to blocked or ready (depending).
// Side effect:
//  The global variable kernel->currentThread becomes nextThread.
//
//  "nextThread" is the thread to be put into the CPU.
//  "finishing" is set if the current thread is to be deleted
//      once we're no longer running on its stack
//      (when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread* nextThread, bool finishing) {
    Thread* oldThread = kernel->currentThread;

    if (kernel->dumpLogToFile) {
        kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << nextThread->getID() <<
                         " is now selected for execution" << endl;
        kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << oldThread->getID() <<
                         " is replaced, and it has executed " << oldThread->getLastTick() << endl;
    } else {
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << nextThread->getID() <<
             " is now selected for execution" << endl;
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << oldThread->getID() <<
             " is replaced, and it has executed " << oldThread->getLastTick() << endl;
    }

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {    // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL) { // if this thread is a user program,
        oldThread->SaveUserState();     // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow();         // check if the old thread
    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();       // check if thread we were running
    // before this one has finished
    // and needs to be cleaned up

    if (oldThread->space != NULL) {     // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
//  If the old thread gave up the processor because it was finishing,
//  we need to delete its carcass.  Note we cannot delete the thread
//  before now (for example, in Thread::Finish()), because up to this
//  point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed() {
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
//  Print the scheduler state -- in other words, the contents of
//  the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print() {
    // cout << "Ready list contents:\n";
    // readyList->Apply(ThreadPrint);
    cout << "Ready list contents:\n";
    cout << "L1:\n";
    L[1]->Apply(ThreadPrint);
    cout << "L2:\n";
    L[2]->Apply(ThreadPrint);
    cout << "L3:\n";
    L[3]->Apply(ThreadPrint);
}

int
Scheduler::maintainQueues() {
    List <Thread*>* temp1;
    List <Thread*>* temp2;
    temp1 = new List <Thread*>();
    temp2 = new List <Thread*>();

    bool L2_new = FALSE;
    bool L1_new = FALSE;

    for (ListIterator<Thread*> it(L[3]); !it.IsDone(); it.Next()) {
        if (it.Item()-> getPriority() >= 50) {

            if (kernel->dumpLogToFile) {
                kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                                 << " is removed from queue L3" << endl;
                kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                                 << " is inserting into queue L" << 3 - it.Item()->getPriority() / 50 << endl;
            } else {
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                     << " is removed from queue L3" << endl;
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                     << " is inserting into queue L" << 3 - it.Item()->getPriority() / 50 << endl;
            }

            L[2]->Append(it.Item());
            temp1->Append(it.Item());
            L2_new = TRUE;
        }
    }

    for (ListIterator<Thread*> it(temp1); !it.IsDone(); it.Next()) {
        L[3]->Remove(it.Item());
    }

    for (ListIterator<Thread*> it(L[2]); !it.IsDone(); it.Next()) {
        if (it.Item()-> getPriority() >= 100) {

            if (kernel->dumpLogToFile) {
                kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                                 << " is removed from queue L2" << endl;
                kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                                 << " is inserting into queue L" << 3 - it.Item()->getPriority() / 50 << endl;
            } else {
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                     << " is removed from queue L2" << endl;
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " << it.Item()->getID()
                     << " is inserting into queue L" << 3 - it.Item()->getPriority() / 50 << endl;
            }

            L[1]->Append(it.Item());
            temp2->Append(it.Item());
            L1_new = TRUE;
        }
    }

    for (ListIterator<Thread*> it(temp2); !it.IsDone(); it.Next()) {
        L[2]->Remove(it.Item());
    }

    if (L1_new) {
        return 1;
    } else if (L2_new) {
        return 2;
    } else {
        return 0;
    }
}

void
Scheduler::incTickToThreads(int amount) {
    for (int i = 1; i <= 3; ++i) {
        for (ListIterator<Thread*> it(L[i]); !it.IsDone(); it.Next()) {
            if (it.Item() != kernel->currentThread) {
                it.Item()->incTickWaited(1);
            }
        }
    }
}

void
Scheduler::preprocessThreads() {
    // for (int i = 1; i <= 3; ++i) {
    //     for (ListIterator<Thread*> it(L[i]); !it.IsDone(); it.Next()) {
    //         if (it.Item() != kernel->currentThread) {
    //             it.Item()->incTickWaited(kernel->currentThread->getTimeUsed());
    //         }
    //     }
    // }
    kernel->currentThread->calNewExecuteTime();
    kernel->currentThread->saveLastTick();
    kernel->currentThread->setTimeUsed(0);
}

Thread*
Scheduler::findNext() {
    if (L[1]->IsEmpty() && L[2]->IsEmpty() && L[3]->IsEmpty()) {
        return NULL;
    }

    preprocessThreads();
    Thread* result;

    if ((result = findNextL1()) != NULL) {
        L[1]->Remove(result);

        if (kernel->dumpLogToFile) {
            kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << result->getID()
                             << " is removed from queue L1" << endl;
        } else {
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " << result->getID()
                 << " is removed from queue L1" << endl;
        }

        return result;
    } else if ((result = findNextL2()) != NULL) {
        L[2]->Remove(result);

        if (kernel->dumpLogToFile) {
            kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << result->getID()
                             << " is removed from queue L2" << endl;
        } else {
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " << result->getID()
                 << " is removed from queue L2" << endl;
        }

        return result;
    } else if ((result = findNextL3()) != NULL) {
        L[3]->Remove(result);

        if (kernel->dumpLogToFile) {
            kernel->dumpfile << "Tick " << kernel->stats->totalTicks << ": Thread " << result->getID()
                             << " is removed from queue L3" << endl;
        } else {
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " << result->getID()
                 << " is removed from queue L3" << endl;
        }

        return result;
    } else {
        return NULL;
    }
}

Thread*
Scheduler::findNextL1() {
    int min_burst = 0x7fffffff;
    Thread* result = NULL;

    for (ListIterator<Thread*> it(L[1]); !it.IsDone(); it.Next()) {
        // operation on it->Item()
        if (it.Item()->getExecutionTime() < min_burst) {
            min_burst = it.Item()->getExecutionTime();
            result = it.Item();
        }
    }

    return result;
}

Thread*
Scheduler::findNextL2() {
    int max_priority = -1;
    Thread* result = NULL;

    for (ListIterator<Thread*> it(L[2]); !it.IsDone(); it.Next()) {
        // operation on it->Item()
        if (it.Item()->getPriority() > max_priority) {
            max_priority = it.Item()->getPriority();
            result = it.Item();
        }
    }

    return result;
}

Thread*
Scheduler::findNextL3() {
    if (L[3]->IsEmpty()) {
        return NULL;
    } else {
        return L[3]->Front();
    }
}

bool
Scheduler::getDirty() const {
    return this->dirty;
}

void
Scheduler::setDirty(bool dirty) {
    this->dirty = dirty;
}
