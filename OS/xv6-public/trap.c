#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  SETGATE(idt[128], 1, SEG_KCODE<<3, vectors[128], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
#ifdef MLFQ_SCHED
  char priorityBoostingFlag = 0;
#endif
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
#if !defined(MULTILEVEL_SCHED) && !defined(MLFQ_SCHED)
    CURTHD(myproc())->tf = tf;
#else
    myproc()->tf = tf;
#endif
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
#ifdef MLFQ_SCHED
      if(ticks % 100 == 0) {
        priorityBoostingFlag = 1;
        if(ticks == (uint)4294967200)
          ticks = 0;
      }
#endif
      wakeup(&ticks);
      release(&tickslock);        
    }
#ifdef MLFQ_SCHED
    acquire_ptable_lock();
    if(priorityBoostingFlag){
      priority_boosting();
    }
    else{
      struct proc *p = myproc();
      if(p && (p->state == RUNNING) && (p->levelOfQueue < MLFQ_K)){
        p->ticks++;
        if((p->ticks) >= (4 * p->levelOfQueue + 2)){
          p->levelOfQueue++;
          p->ticks = 0;
        }
      }
    }
    release_ptable_lock();
#endif
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case 128: 
    cprintf("user interrupt 128 called!\n");
    exit();

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
#ifdef MULTILEVEL_SCHED
  if(myproc() && myproc()->levelOfQueue == 0 && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();
#elif MLFQ_SCHED
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER && myproc()->ticks == 0) {
    yield();
  }
#else
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER) {
    yield();
  }
#endif
  
  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
