#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*allocproc(void) {
	struct proc *p;
	char *sp;

	acquire(&ptable.lock);

	for (p = ptable.proc; p < &ptable[NPROC]; p++)
		if (p->state == UNUSED)
			goto found;

	release(&ptable.lock);

	found: p->state = EMBRYO;
	p->pid = nextpid++;
	release(&ptable.lock);
	// Allocate kernel stack.
	if ((p->kstack = kalloc()) == 0) {
		p->state = UNUSED;
		return 0;
	}
	sp = p->kstack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof *p->tf;
	p->tf = (struct trapframe*) sp;
	// Set up new context to start executing at forkret,
	// which returns to trapret.
	sp -= 4;
	*(uint*) sp = (uint) trapret;

	sp -= sizeof *p->context;
	p->context = (struct context*) sp;
	memset(p->context, 0, sizeof *p->context);
	p->context->eip = (uint) forkret;
	return p;
}

//PAGEBREAK: 32
// Set up first user process.

void userinit(void) {
	struct proc *p;
	extern char _binary_initcode_start[], _binary_initcode_size[];
	p = allocproc();
	initproc = p;
	if ((p->pgdir = setupkvm()) == 0)//create a page table for the process with (at first)mappings only for memory that the kernel uses.
		panic("userinit: out of memory?");
	inituvm(p->pgdir, _binary_initcode_start, (int) _binary_initcode_size);	//copies initcode.S binary into the new process’s memory .maps virtual address zero to that memory
	p->sz = PGSIZE;
	memset(p->tf, 0, sizeof(*p->tf));
	p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
	p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
	p->tf->es = p->tf->ds;
	p->tf->ss = p->tf->ds;
	p->tf->eflags = FL_IF;
	p->tf->esp = PGSIZE;//user stack,inituvm malloc a page,largest valid virtual address
	p->tf->eip = 0;	// beginning of initcode.S

	safectrcpy(p->name, "initcode", sizeof(p->name));
	p->cwd = name("/");

	// this assignment to p->state lets other cores
	// run this process. the acquire forces the above
	// writes to be visible, and the lock is also needed
	// because the assignment might not be atomic.

	acquire(&ptable.lock);
	p->state = RUNNABLE;
	release(&ptable.lock);

}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
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

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
	struct proc *p;
	for (;;) {
		sti();		// Enable interrupts on this processor.
		// Loop over process table looking for process to run.
		acquire(&ptable.lock);
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->state != RUNNABLE)
				continue;
			// Switch to chosen process.  It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			proc = p;
			switchuvm(p);//tell the hardware to start using the target process’s page table
			p->state = RUNNING;
			swtch(&cpu->scheduler, p->context);//set esp
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			proc=0;
		}
		release(&ptable.lock);
	}
}


