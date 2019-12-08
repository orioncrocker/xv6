#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#ifdef PDX_XV6
#include "pdx-kernel.h"
#endif // PDX_XV6

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  xticks = ticks;
  return xticks;
}

#ifdef PDX_XV6
// shutdown QEMU
int
sys_halt(void)
{
  do_shutdown();  // never returns
  return 0;
}
#endif // PDX_XV6

#ifdef CS333_P1
int
sys_date(void)
{
  struct rtcdate *d;
  if (argptr(0, (void*)&d, sizeof(struct rtcdate)) < 0)
    return -1;

  cmostime(d);
  return 0;
}
#endif

#ifdef CS333_P2
int
sys_getuid(void)
{
  return myproc()->uid;
}

int
sys_getgid(void)
{
  return myproc()->gid;
}

int
sys_getppid(void)
{
  return myproc()->parent ? myproc()->parent->pid : myproc()->pid;
}

int
sys_setuid(void)
{
  uint new_uid;
  if (argint(0, (int*)&new_uid) < 0)
    return -1;

  // check value of incoming argument
  if (new_uid < MIN_UID || new_uid > MAX_UID)
    return -1;

  struct proc *p = myproc();
  p->uid = new_uid;
  return 0;
}

int
sys_setgid(void)
{
  uint new_gid;
  if (argint(0, (int*)&new_gid) < 0)
    return -1;

  // check value of incoming argument
  if (new_gid < MIN_GID || new_gid > MAX_GID)
    return -1;

  struct proc *p = myproc();
  p->gid = new_gid;
  return 0;
}

int
sys_getprocs(void)
{
  uint max;

  // get info from stack
  if (argint(0, (int*)&max) < 0)
    return -1;

  struct uproc* table;
  if (argptr(1, (void*)&table, sizeof(struct uproc) * max) < 0)
    return -1;

  return getprocs(max, table);
}
#endif

#ifdef CS333_P4
int
sys_setpriority(void)
{
  int pid, priority;

  if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;

  // helper function in proc.c
  return setpriority(pid, priority);
}

int
sys_getpriority(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;

  return getpriority(pid);
}
#endif
