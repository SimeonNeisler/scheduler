/*****************************************************************
*       proc.c - simplified for CPSC405 Lab by Gusty Cooper, University of Mary Washington
*       adapted from MIT xv6 by Zhiyi Huang, hzy@cs.otago.ac.nz, University of Otago
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include "defs.h"
#include "proc.h"

const int niceVals[40] = {-20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

static void wakeup1(int chan);

// Dummy lock routines. Not needed for lab
void acquire(int *p) {
    return;
}

void release(int *p) {
    return;
}

// enum procstate for printing
char *procstatep[] = { "UNUSED", "EMPRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE" };

// Table of all processes
struct {
  int lock;   // not used in Lab
  struct proc proc[NPROC];
} ptable;

// Initial process - ascendent of all other processes
static struct proc *initproc;

// Used to allocate process ids - initproc is 1, others are incremented
int nextpid = 1;

// Funtion to use as address of proc's PC
void
forkret(void)
{
}

// Funtion to use as address of proc's LR
void
trapret(void)
{
}

// Initialize the process table
void
pinit(void)
{
  memset(&ptable, 0, sizeof(ptable));
}

// Look in the process table for a process id
// If found, return pointer to proc
// Otherwise return 0.
static struct proc*
findproc(int pid)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->pid == pid)
      return p;
  return 0;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  p->context = (struct context*)malloc(sizeof(struct context));
  memset(p->context, 0, sizeof *p->context);
  p->context->pc = (uint)forkret;
  p->context->lr = (uint)trapret;

  return p;
}

// Set up first user process.
/*This function was updated to give the default process a default niceness index of 20, a default niceness of 0, a default weight of 1024, a default virtual run time of 0 and a default time slice of 0 
*/
int
userinit(void)
{
  struct proc *p;
  p = allocproc();
  initproc = p;
  p->sz = PGSIZE;
  strcpy(p->cwd, "/");
  strcpy(p->name, "userinit"); 
  p->state = RUNNING;
  p->niceIndex = 20;
  p->nice = niceVals[p->niceIndex];
  p->weight = 1024;
  p->vruntime = 0;
  p->timeslice = 0;
  curr_proc = p;

  return p->pid;
}

/*
  This function can be used to set the niceness value of a process, by passing in the process id, and the desired niceness value, and then recalculates the process' new weight.
*/
void setNice(int pid, int nice) {
  struct proc *p = findproc(pid);
  if(p != 0) {
    p->niceIndex = nice+20;
    p->nice = niceVals[p->niceIndex];
    p->weight = 1024/(pow(1.25, p->nice));
  }
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.

/*This function was updated to give new process' a default niceness index (the position in the nicevals array that they should hold), a default niceness, a default weight, a default time slice (of 0) and a default virtual run time that is equivalent to the average virtual run time of all process' in the process table
*/
int
Fork(int fork_proc_id)
{
  int pid;
  struct proc *np, *fork_proc;

  // Find current proc
  if ((fork_proc = findproc(fork_proc_id)) == 0)
    return -1;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  np->sz = fork_proc->sz;
  np->parent = fork_proc;
  // Copy files in real code
  strcpy(np->cwd, fork_proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  strcpy(np->name, fork_proc->name);
  np->niceIndex = 20;
  np->nice = niceVals[np->niceIndex];
  np->weight = 1024;
  np->vruntime = 0;
  np->timeslice = 0;
  int numProcs = 0;
  int totalVrt = 0;
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid) {
      totalVrt += p->vruntime;
      numProcs++;
    }
  }
  np->vruntime = (totalVrt/(numProcs-1));
  return pid;
}

/*This process calculates the total weight of all running and runnable process' in the process table*/

int sumWeight() {
  struct proc *p;
  int totalWeight = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == RUNNABLE || p->state == RUNNING) {
      totalWeight += p->weight;
    }
  }
  return totalWeight;
}


/*This process calculates the time slice of a process given its pid
    It starts by locating a process in the table given its pid
    Then it calculates the time slice for this process by (the process' weight * schedule latency)/the total weight of all running or runnable process'
    If the calculated time slice is less than the minimum granularity it simply return the minimum granularity
*/
int calcTimeSlice(int pid) {
  int totalWeight = sumWeight();
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid == pid) {
      break;
    }
  }
  int time_slice = (p->weight * LATENCY/totalWeight);
  if(time_slice < MIN_GRAN) {
    time_slice = MIN_GRAN;
  }
  printf("time slice: %d\n", time_slice);
  return time_slice;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
int
Exit(int exit_proc_id)
{
  struct proc *p, *exit_proc;

  // Find current proc
  if ((exit_proc = findproc(exit_proc_id)) == 0)
    return -2;

  if(exit_proc == initproc) {
    printf("initproc exiting\n");
    return -1;
  }

  // Close all open files of exit_proc in real code.

  acquire(&ptable.lock);

  wakeup1(exit_proc->parent->pid);

  // Place abandoned children in ZOMBIE state - HERE
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == exit_proc){
      p->parent = initproc;
      p->state = ZOMBIE;
    }
  }

  exit_proc->state = ZOMBIE;

  // sched();
  release(&ptable.lock);
  return 0;
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
// Return -2 has children, but not zombie - must keep waiting
// Return -3 if wait_proc_id is not found
int
Wait(int wait_proc_id)
{
  struct proc *p, *wait_proc;
  int havekids, pid;

  // Find current proc
  if ((wait_proc = findproc(wait_proc_id)) == 0)
    return -3;

  acquire(&ptable.lock);
  for(;;){ // remove outer loop
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != wait_proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        p->kstack = 0;
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || wait_proc->killed){
      release(&ptable.lock);
      return -1;
    }
    if (havekids) { // children still running
      Sleep(wait_proc_id, wait_proc_id);
      release(&ptable.lock);
      return -2;
    }

  }
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
int
Sleep(int sleep_proc_id, int chan)
{
  struct proc *sleep_proc;
  // Find current proc
  if ((sleep_proc = findproc(sleep_proc_id)) == 0)
    return -3;

  sleep_proc->chan = chan;
  sleep_proc->state = SLEEPING;
  return sleep_proc_id;
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(int chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}


void
Wakeup(int chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}



// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
Kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
// A continous loop in real code
//  if(first_sched) first_sched = 0;
//  else sti();

  curr_proc->state = RUNNABLE;

  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p == curr_proc || p->state != RUNNABLE)
      continue;

    // Switch to chosen process.
    curr_proc = p;
    p->state = RUNNING;
    break;
  }
  release(&ptable.lock);

}

/* Round robin scheduler
 * Checks if current running process is at the end of the process table
      If so it starts iterating through the table from the beginning to find the first runnable process, and then sets current process equal to that

      If not it finds the current process in the process table and looks for any runnable process after it in the table.
      If it can't find any it goes back to the beginning of the table to find hte first runnable process before it.

      In case it can't find any runnable process it simply keeps the current process as running.
*/
int round_robin() {

  struct proc *p;

  acquire(&ptable.lock);
  
  if(curr_proc == &ptable.proc[NPROC]) {
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if(p->state != RUNNABLE) {
        continue;
      } else {
        if(curr_proc->state == RUNNING)
          curr_proc->state = RUNNABLE;
        curr_proc = p;
        curr_proc->state = RUNNING;
        break;
      }
    }
  } else {
    for(p = curr_proc+1; p < &ptable.proc[NPROC]; p++) {
      if(p->state == RUNNABLE) {
        if(curr_proc->state == RUNNING)
          curr_proc->state = RUNNABLE;
        curr_proc = p;
        curr_proc->state = RUNNING;
        break;
      }
    }
    if(p == &ptable.proc[NPROC]) {
      p = ptable.proc;
      while(p != curr_proc) {
        if(p->state == RUNNABLE) {
          if(curr_proc->state == RUNNING)
            curr_proc->state = RUNNABLE;
          curr_proc = p;
          curr_proc->state = RUNNING;
          break;
        }
        p++;
      }
    }
  }
  release(&ptable.lock);
  return curr_proc->pid;
}



/* Linux Completely Fair Scheduler
      Loops through the process table from the beginning to find the process with the lowest virtual run time (VRT)
        If two process tie for lowest VRT it selects the one with the highest weight
      Updates current process to the found process, and calculates that process' time slice (see calcTimeSlice for details) and then updates that process' virtual run time by incrementing it by (1024 * the calculated time slice)/the current process' weight
*/
int lcfs() {
  struct proc *p;
  struct proc *lowest;
  lowest = curr_proc;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == RUNNABLE) {
      if(p->vruntime < lowest->vruntime) {
        lowest = p;
      }
      if(p->vruntime == lowest->vruntime) {
        if(lowest->weight < p->weight) {
          lowest = p;
        }
      }
    }
  }
  curr_proc->state = RUNNABLE;
  curr_proc = lowest;
  curr_proc->state = RUNNING;
  curr_proc->timeslice =calcTimeSlice(lowest->pid);
  curr_proc->vruntime += (1024 * curr_proc->timeslice/curr_proc->weight);
  release(&ptable.lock);
  return curr_proc->pid;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
/*This method was updated to also print out a process' niceness, weight, virtual run time, and time slice, */
void
procdump(void)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->pid > 0)
      printf("pid: %d, parent: %d state: %s\t nice: %d\t weight: %d \tvrt: %d\t timeslice: %d\n", p->pid, p->parent == 0 ? 0 : p->parent->pid, procstatep[p->state], p->nice, p->weight, p->vruntime, p->timeslice);
}


