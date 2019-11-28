#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#ifdef CS333_P4
static int promoteprocs();
#endif
#ifdef CS333_P3
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
// provided helper functions
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int  stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc*, enum procstate, const char *, int);
#endif

static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  #ifdef CS333_P3
  struct ptrs list[statecount];
  #endif
  #ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
  #endif
} ptable;

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
#ifdef CS333_P3
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  // acquire spin lock
  acquire(&ptable.lock);

  // get head of unused list
  p = ptable.list[UNUSED].head;
  if(p == NULL){
    release(&ptable.lock);
    return 0;
  }

  // remove from UNUSED list
  if(stateListRemove(&ptable.list[UNUSED], p) < 0)
    panic("Process is not in UNUSED list. allocproc");
  assertState(p, UNUSED, "allocproc", 116);
  p->state = EMBRYO;
  p->pid = nextpid++;

  // add to EMBRYO list
  stateListAdd(&ptable.list[EMBRYO], p);
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    // if could not allocate kernel stack, return to UNUSED list
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[EMBRYO], p) < 0)
      panic("Process is not in EMBRYO list. allocproc");
    assertState(p, EMBRYO, "allocproc", 130);
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
    release(&ptable.lock);
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  // assign number of ticks to start_ticks
  p->start_ticks = ticks;
  // initialize both cpu_ticks values to 0
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;

  #ifdef CS333_P4
  p->priority = MAXPRIO;
  p->budget = DEFAULT_BUDGET;
  #endif

  return p;
}
#else
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  // acquire spin lock
  acquire(&ptable.lock);

  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    // release spin lock
    release(&ptable.lock);
    return 0;
  }

  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  #ifdef CS333_P1
  // assign number of ticks to start_ticks
  p->start_ticks = ticks;
  #endif
  #ifdef CS333_P2
  // initialize both cpu_ticks values to 0
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;
  #endif

  return p;
}
#endif

//PAGEBREAK: 32
// Set up first user process.
#ifdef CS333_P3
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();

  #ifdef CS333_P4
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  #endif

  release(&ptable.lock);

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->parent = 0;
  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  // remove proc from EMBRYO list
  if(stateListRemove(&ptable.list[EMBRYO], p) < 0)
    panic("Process is not in EMBRYO list. userinit");

  // assert that process is actually an EMBRYO
  assertState(p, EMBRYO, "userinit", 224);
  p->state = RUNNABLE;

  #ifdef CS333_P4
  stateListAdd(&ptable.ready[MAXPRIO], p);
  #else
  stateListAdd(&ptable.list[RUNNABLE], p);
  #endif

  release(&ptable.lock);
}
#else
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  #ifdef CS333_P2
  p->parent = 0;
  #endif

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
}
#endif

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
#ifdef CS333_P3
int
fork(void)
{
  int i;
  uint pid;
  struct proc *newproc;
  struct proc *curproc = myproc();

  // Allocate process.
  if((newproc = allocproc()) == 0)
    return -1;

  // Copy process state from proc.
  if((newproc->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(newproc->kstack);
    newproc->kstack = 0;

    acquire(&ptable.lock);

    if(stateListRemove(&ptable.list[EMBRYO], newproc) < 0)
      panic("Process is not in EMBRYO list. fork");
    assertState(newproc, EMBRYO, "fork", 296);
    newproc->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], newproc);

    release(&ptable.lock);
    return -1;
  }

  newproc->sz = curproc->sz;
  newproc->parent = curproc;
  *newproc->tf = *curproc->tf;
  newproc->uid = curproc->uid;
  newproc->gid = curproc->gid;

  // Clear %eax so that fork returns 0 in the child.
  newproc->tf->eax = 0;

  for(i = 0; i < NOFILE; i++) {
    if(curproc->ofile[i])
      newproc->ofile[i] = filedup(curproc->ofile[i]);
  }
  newproc->cwd = idup(curproc->cwd);
  safestrcpy(newproc->name, curproc->name, sizeof(curproc->name));
  pid = newproc->pid;

  acquire(&ptable.lock);

  if(stateListRemove(&ptable.list[EMBRYO], newproc) < 0)
    panic("Process is not in EMBRYO list. fork");
  assertState(newproc, EMBRYO, "fork", 333);
  newproc->state = RUNNABLE;

  #ifdef CS333_P4
  stateListAdd(&ptable.ready[MAXPRIO], newproc);
  #else
  stateListAdd(&ptable.list[RUNNABLE], newproc);
  #endif

  release(&ptable.lock);

  return pid;
}
#else
int
fork(void)
{
  int i;
  uint pid;
  struct proc *newproc;
  struct proc *curproc = myproc();

  // Allocate process.
  if((newproc = allocproc()) == 0)
    return -1;

  // Copy process state from proc.
  if((newproc->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(newproc->kstack);
    newproc->kstack = 0;
    newproc->state = UNUSED;
    return -1;
  }

  newproc->sz = curproc->sz;
  newproc->parent = curproc;
  *newproc->tf = *curproc->tf;

  #ifdef CS333_P2
  newproc->uid = curproc->uid;
  newproc->gid = curproc->gid;
  #endif

  // Clear %eax so that fork returns 0 in the child.
  newproc->tf->eax = 0;

  for(i = 0; i < NOFILE; i++) {
    if(curproc->ofile[i])
      newproc->ofile[i] = filedup(curproc->ofile[i]);
  }
  newproc->cwd = idup(curproc->cwd);
  safestrcpy(newproc->name, curproc->name, sizeof(curproc->name));
  pid = newproc->pid;

  acquire(&ptable.lock);
  newproc->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}
#endif

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P3
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Comb through all lists and pass abandoned children to initproc
  for(int i = EMBRYO; i <= ZOMBIE; i++){
    p = ptable.list[i].head;
    while(p != NULL){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
      p = p->next;
    }
  }

  if(stateListRemove(&ptable.list[RUNNING], curproc) < 0)
    panic("Process is not in RUNNING list. exit");
  assertState(curproc, RUNNING, "exit", 505);

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);

  #ifdef PDX_XV6
  curproc->sz = 0;
  #endif // PDX_XV6

  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  #ifdef PDX_XV6
  curproc->sz = 0;
  #endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifdef CS333_P3
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(int i = EMBRYO; i <= ZOMBIE; i++){
      p = ptable.list[i].head;

      while(p != NULL){
        if (p->parent != curproc){
          p = p->next;
          continue;
        }

        havekids = 1;
        if(p->state == ZOMBIE){
          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;

          if(stateListRemove(&ptable.list[ZOMBIE], p) < 0)
            panic("Process is not in ZOMBIE list. wait");
          assertState(p, ZOMBIE, "wait", 604);

          p->state = UNUSED;
          stateListAdd(&ptable.list[UNUSED], p);

          release(&ptable.lock);
          return pid;
        }
        p = p->next;
      }
    }
    #ifdef CS333_P4
    // Scan through all ready lists
    for(int i = 0; i <= MAXPRIO; i++){
      p = ptable.ready[i].head;

      while(p != NULL){
        if(p->parent != curproc){
          p = p->next;
          continue;
        }

        havekids = 1;
        if(p->state == ZOMBIE){
          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;

          if(stateListRemove(&ptable.list[ZOMBIE], p) < 0)
            panic("Process is not in ZOMBIE list. wait");
          assertState(p, ZOMBIE, "wait", 604);

          p->state = UNUSED;
          stateListAdd(&ptable.list[UNUSED], p);

          release(&ptable.lock);
          return pid;
        }
        p = p->next;
      }
    }
    #endif

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

#ifdef CS333_P3
// new and fancy scheduler
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  #ifdef PDX_XV6
  int idle;  // for checking if processor is idle
  #endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

    #ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
    #endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    #ifdef CS333_P4
    // Check if it is time to promote all ready procs
    if(ticks >= ptable.PromoteAtTime){
      promoteprocs();

      // update promote time
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }

    // find a proc from one of the priority lists, starting with highest priority
    /*
    for(int i = MAXPRIO; i >= 0 && p != NULL; i--)
      p = ptable.ready[i].head;
    */
    p = ptable.ready[MAXPRIO].head;

    #else

    p = ptable.list[RUNNABLE].head;
    #endif

    if(p != NULL){

      #ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
      #endif // PDX_XV6
      c->proc = p;
      switchuvm(p);

      #ifdef CS333_P4
      if (stateListRemove(&ptable.ready[p->priority], p) < 0)
        panic("Process is not in correct READY list. scheduler");
      #else
      if (stateListRemove(&ptable.list[RUNNABLE], p) < 0)
        panic("Process is not in RUNNABLE list. scheduler");
      #endif
      assertState(p, RUNNABLE, "scheduler", 571);
      p->state = RUNNING;

      stateListAdd(&ptable.list[RUNNING], p);

      p->cpu_ticks_in = ticks;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }

    release(&ptable.lock);

    #ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
    #endif // PDX_XV6
  }
}
#else
// old n' busted scheduler
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  #ifdef PDX_XV6
  int idle;  // for checking if processor is idle
  #endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

    #ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
    #endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      #ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
      #endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks;
      #endif
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
    #ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
    #endif // PDX_XV6
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  #ifdef CS333_P2
  p->cpu_ticks_total += (ticks - p->cpu_ticks_in);
  #endif

  /*
  #ifdef CS333_P4
  p->budget = p->budget - (p->cpu_ticks_total - p->cpu_ticks_in);
  // check if process needs to be demoted
  if(p->budget <= 0) {
    if(p->priority > 0)
      p->priority--;
    p->budget = DEFAULT_BUDGET;
  }
  #endif
  */

  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
#ifdef CS333_P3
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  if(stateListRemove(&ptable.list[RUNNING], curproc) < 0)
    panic("Process is not in RUNNING list. yield");
  assertState(curproc, RUNNING, "yield", 724);
  curproc->state = RUNNABLE;
  #ifdef CS333_P4
  // calculate new budget
  curproc->budget = curproc->budget - (curproc->cpu_ticks_total - curproc->cpu_ticks_in);
  // determine if proc needs to be demoted
  if(curproc->budget <= 0) {
    if(curproc->priority > 0)
      curproc->priority--;
    curproc->budget = DEFAULT_BUDGET;
  }
  stateListAdd(&ptable.ready[curproc->priority], curproc);
  #else
  stateListAdd(&ptable.list[RUNNABLE], curproc);
  #endif
  sched();
  release(&ptable.lock);
}
#else
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#ifdef CS333_P3
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;

  if(stateListRemove(&ptable.list[RUNNING], p) < 0)
    panic("Process is not in RUNNING list. sleep");

  assertState(p, RUNNING, "sleep", 790);

  p->state = SLEEPING;
  stateListAdd(&ptable.list[SLEEPING], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P3
static void
wakeup1(void *chan)
{
  struct proc *p = ptable.list[SLEEPING].head;
  struct proc *temp;

  while(p != NULL){
    if(p->state == SLEEPING && p->chan == chan){

      temp = p->next;

      if(stateListRemove(&ptable.list[SLEEPING], p) < 0)
        panic("Process is not in SLEEPING list. wakeup1");
      assertState(p, SLEEPING, "wakeup1", 833);

      p->state = RUNNABLE;
      #ifdef CS333_P4
      // calculate new budget
      p->budget = p->budget - (p->cpu_ticks_total - p->cpu_ticks_in);
      // determine if proc needs to be demoted
      if(p->budget <= 0) {
        if(p->priority > 0)
          p->priority--;
        p->budget = DEFAULT_BUDGET;
      }
      stateListAdd(&ptable.ready[p->priority], p);
      #else
      stateListAdd(&ptable.list[RUNNABLE], p);
      #endif

      p = temp;
    }
    else
      p = p->next;
  }
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifdef CS333_P3
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);

  // comb through all lists looking for pid to inform it that it's dead
  for(int i = UNUSED; i <= ZOMBIE; i++){
    p = ptable.list[i].head;
    while(p != NULL){
      if(p->pid == pid){
        p->killed = 1;

        // Wake process from sleep if necessary.
        if(p->state == SLEEPING){
          if(stateListRemove(&ptable.list[SLEEPING], p) < 0)
            panic("Process is not in SLEEPING list. kill");
          assertState(p, SLEEPING, "kill", 884);
          p->state = RUNNABLE;
          stateListAdd(&ptable.list[RUNNABLE], p);
        }

        release(&ptable.lock);
        return 0;
      }
      // the search continues
      p = p->next;
    }
  }

  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#ifdef CS333_P4
void
procdumpP4(struct proc *p, char *state)
{
  uint ppid = p->parent ? p->parent->pid : p->pid;

  // calculate elapsed time
  uint total = ticks - p->start_ticks;
  uint total_s = total / 1000;
  uint total_ms = total % 1000;
  char *total_zeros = "";
  if (total_ms < 10)
    total_zeros = "00";
  else if (total_ms < 100)
    total_zeros = "0";

  // calculate cpu time
  uint cpu_time = p->cpu_ticks_total;
  uint cpu_s = cpu_time / 1000;
  uint cpu_ms = cpu_time % 1000;
  char* cpu_zeros = "";
  if (cpu_ms < 10)
    cpu_zeros = "00";
  else if (cpu_ms < 100)
    cpu_zeros = "0";

  //#define HEADER "\nPID\tName         UID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t PCs\n"
  cprintf("%d\t%s\t     %d\t\t%d\t%d\t%d\t%d.%s%d\t%d.%s%d\t%s\t%d\t",
          p->pid, p->name, p->uid, p->gid, ppid, p->priority, total_s,
          total_zeros, total_ms, cpu_s, cpu_zeros, cpu_ms, state, p->sz);

}
#elif defined (CS333_P2)
void
procdumpP2(struct proc *p, char *state)
{
  uint ppid = p->parent ? p->parent->pid : p->pid;

  // calculate elapsed time
  uint total = ticks - p->start_ticks;
  uint total_s = total / 1000;
  uint total_ms = total % 1000;
  char *total_zeros = "";
  if (total_ms < 10)
    total_zeros = "00";
  else if (total_ms < 100)
    total_zeros = "0";

  // calculate cpu time
  uint cpu_time = p->cpu_ticks_total;
  uint cpu_s = cpu_time / 1000;
  uint cpu_ms = cpu_time % 1000;
  char* cpu_zeros = "";
  if (cpu_ms < 10)
    cpu_zeros = "00";
  else if (cpu_ms < 100)
    cpu_zeros = "0";

  cprintf("%d\t%s\t     %d  \t%d\t%d\t%d.%s%d\t%d.%s%d\t%s\t%d\t",
          p->pid, p->name, p->uid, p->gid, ppid, total_s, total_zeros,
          total_ms, cpu_s, cpu_zeros, cpu_ms, state, p->sz);
}
#elif defined (CS333_P1)
void
procdumpP1(struct proc *p, char *state)
{
  uint calcTicks = ticks - p->start_ticks;
  uint seconds = calcTicks / 1000;
  uint milli = calcTicks % 1000;

  cprintf("%d\t%s\t     %d.", p->pid, p->name, seconds);
  if (milli < 10)
  	cprintf("00");
  else if (milli < 100)
  	cprintf("0");
  cprintf("%d\t%s\t%d\t", milli, state, p->sz);
}
#endif

void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  #if defined(CS333_P4)
  #define HEADER "\nPID\tName         UID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t PCs\n"
  #elif defined(CS333_P3)
  #define HEADER "\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
  #elif defined(CS333_P2)
  #define HEADER "\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
  #elif defined(CS333_P1)
  #define HEADER "\nPID\tName         Elapsed\tState\tSize\t PCs\n"
  #else
  #define HEADER "\n"
  #endif

  cprintf(HEADER);  // not conditionally compiled as must work in all project states

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    #if defined(CS333_P4)
    procdumpP4(p, state);
    #elif defined(CS333_P3)
    procdumpP2(p, state);
    #elif defined(CS333_P2)
    procdumpP2(p, state);
    #elif defined(CS333_P1)
    procdumpP1(p, state);
    #else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
    #endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
  #ifdef CS333_P1
  cprintf("$ ");  // simulate shell prompt
  #endif // CS333_P1
}

#ifdef CS333_P2
int
getprocs(uint max, struct uproc* table) {
  uint i = 0;
  struct proc *p;

  // acquire spin lock
  acquire(&ptable.lock);

  // fill values for all table elements
  for (p = ptable.proc; p < &ptable.proc[NPROC] && i < max; p++) {

    // only copy data of running or sleeping processes
    if (p->state == UNUSED || p->state == EMBRYO)
      continue;

    table[i].pid = p->pid;
    table[i].uid = p->uid;
    table[i].gid = p->gid;
    table[i].ppid = p->parent ? p->parent->pid : p->pid;
    table[i].elapsed_ticks = ticks-p->start_ticks;
    table[i].CPU_total_ticks = p->cpu_ticks_total;
    table[i].size = p->sz;
    safestrcpy(table[i].name, p->name, STRMAX);
    safestrcpy(table[i].state, states[p->state], STRMAX);

    #ifdef CS333_P4
    table[i].priority = p->priority;
    #endif

    ++i;
  }

  // release spin lock
  release(&ptable.lock);
  return i;
}
#endif

#ifdef CS333_P3
// list management helper functions
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}

static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process is not found. return error
  if(current == NULL){
    return -1;
  }

  // Process found.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = NULL;

  return 0;
}

static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
  #ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
  #endif
}

static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}

// example usage:
// assertState(p, UNUSED, __FUNCTION__, __LINE__);
// This code uses gcc preprocessor directives. For details, see
// https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
static void
assertState(struct proc *p, enum procstate state, const char * func, int line)
{
    if (p->state == state)
      return;
    cprintf("Error: proc state is %s and should be %s.\nCalled from %s line %d\n",
        states[p->state], states[state], func, line);
    panic("Error: Process state incorrect in assertState()");
}

// functions that output current state of lists
void
readyList(void)
{
  struct proc *p;
  acquire(&ptable.lock);

  cprintf("Ready List Processes:\n");
  for(p = ptable.list[RUNNABLE].head; p != NULL; p = p->next){
    cprintf("%d", p->pid);
    if(p->next)
      cprintf(" -> ");
  }

  cprintf("\n$ ");
  release(&ptable.lock);
}

void
freeList(void)
{
  struct proc *p;
  int procs = 0;
  acquire(&ptable.lock);

  for(p = ptable.list[UNUSED].head; p != NULL; p = p->next)
    procs++;

  cprintf("Free List Size: %d processes\n$ ", procs);

  release(&ptable.lock);
}

void
sleepList(void)
{
  struct proc *p;
  acquire(&ptable.lock);

  cprintf("Sleep List Processes:\n");
  for(p = ptable.list[SLEEPING].head; p != NULL; p = p->next){
    cprintf("%d", p->pid);
    if(p->next)
      cprintf(" -> ");
  }

  cprintf("\n$ ");
  release(&ptable.lock);
}

void
zombieList(void)
{
  struct proc *p;
  acquire(&ptable.lock);

  cprintf("Zombie List Processes:\n");
  for(p = ptable.list[ZOMBIE].head; p != NULL; p = p->next){
    cprintf("(%d,%d)", p->pid, p->parent ? p->parent->pid : p->pid);
    if(p->next)
      cprintf(" -> ");
  }

  cprintf("\n$ ");
  release(&ptable.lock);
}
#endif

#ifdef CS333_P4
// helper function to prmote ALL procs
int
promoteprocs()
{
  // make sure lock is being held
  if(!holding(&ptable.lock))
    panic("promoteprocs ptable.lock");

  int totalpromoted = 0;
  struct proc *p = NULL;

  for(int i = EMBRYO; i <= ZOMBIE; i++){
    p = ptable.list[i].head;

    // go through list
    while(p != NULL){
      if(p->priority < MAXPRIO){
        p->priority++;
        totalpromoted++;
      }
      p = p->next;
    }
  }

  for(int i = 0; i < MAXPRIO; i++){
    p = ptable.ready[i].head;
    struct proc *next = NULL;

    while(p != NULL) {
      next = p->next;
      if (stateListRemove(&ptable.ready[i], p) < 0)
        panic("Process is not in correct READY list. scheduler");
      assertState(p, RUNNABLE, "promoteprocs", 1506);
      p->priority++;
      stateListAdd(&ptable.ready[p->priority], p);

      p = next;
      totalpromoted++;
    }
  }
  return totalpromoted;
}

struct proc*
findproc(int pid)
{

  // make sure lock is being held
  if(!holding(&ptable.lock))
    panic("findproc ptable.lock");

  struct proc* p = NULL;

  // look through all lists searching for proc
  for(int i = EMBRYO; i <= ZOMBIE; i++){
    p = ptable.list[i].head;

    // go through list
    while(p != NULL){
      // proc found
      if (p->pid == pid)
        return p;
      p = p->next;
    }

  }
  return p;
}

int
setpriority(int pid, int priority)
{
  if (priority < 0 || priority > MAXPRIO || pid < 0)
    return -1;

  struct proc *p = NULL;

  acquire(&ptable.lock);

  // go look for proc
  p = findproc(pid);

  // if process couldn't be found
  if(p == NULL){
    release(&ptable.lock);
    return -1;
  }

  p->priority = priority;
  p->budget = DEFAULT_BUDGET;

  release(&ptable.lock);
  return 0;
}

int
getpriority(int pid)
{
  if (pid < 0)
    return -1;

  struct proc *p = NULL;
  acquire(&ptable.lock);

  p = findproc(pid);
  if (p == NULL){
    release(&ptable.lock);
    return -1;
  }

  release(&ptable.lock);
  return p->priority;
}
#endif
