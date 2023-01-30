#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 base, mask;
  int len;

  argaddr(0, &base);
  argint(1, &len);
  argaddr(2, &mask);

  if(len > 64)
    return -1;

  uint64 mask_kernel = 0;
  struct proc *p = myproc();

  for (int i = 0; i < len; i++) {
    pagetable_t pagetable = p->pagetable;
    pte_t *pte;
    int level = 2;
    for(; level > 0; level--) {
      pte = &pagetable[PX(level, base)];
      if(*pte & PTE_V) {
        pagetable = (pagetable_t)PTE2PA(*pte);
      } else {
        break;
      }
    }
    pte = &pagetable[PX(0, base)];
    if ((level == 0) && (*pte & PTE_V) && (*pte & PTE_A)) {
      mask_kernel |= (1 << i);
      *pte &= ~PTE_A;
    }
    base += PGSIZE;
  }

  if(copyout(p->pagetable, mask, (char *)&mask_kernel, (len + 8 - 1) / 8) < 0)
    return -1;
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
