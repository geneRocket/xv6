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
			swtch(&cpu->scheduler, p->context);			//set esp
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			proc = 0;
		}
		release(&ptable.lock);
	}
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
	int intena;

	if (!hoding(&ptable.lock))
		panic("sched ptable.lock");
	if (cpu->ncli != 1)	//since a lock is held, the CPU should be running with interrupts disabled.
		panic("sched locks");
	if (proc->state == RUNNING)
		panic("sched running");
	if (readflags() & FL_IF)
		panic("sched interruptible");
	intena = cpu->intena;
	swtch(&proc->context, cpu->scheduler);
	cpu->intena = intena;	//belong to a process attribute
}

// Give up the CPU for one scheduling round.
void yield(void) {
	acquire(&ptable.lock);
	proc->state = RUNNABLE;
	sched();
	/*the caller of
	 swtch must already hold the lock, and control of the lock passes to the switched-to
	 code.*/
	release(&ptable.lock);
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
	if (proc == 0)
		panic("sleep");
	if (lk == 0)
		panic("sleep without lk");
	// Must acquire ptable.lock in order to
	// change p->state and then call sched.
	// Once we hold ptable.lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with ptable.lock locked),
	// so it's okay to release lk.
	if (lk != &ptable.lock) {
		acquire(&ptable.lock);
		release(lk);
	}
	// Go to sleep.
	proc->chan = chan;
	proc->state = SLEEPING;
	sched();
	// Tidy up.
	proc->chan = 0;

	// Reacquire original lock.
	if (lk != &ptable.lock) {
		release(&ptable.lock);
		acquire(lk);
	}
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan) {
	struct proc *p;
	for (p = ptable.proc; p < &ptable[NPROC]; p++)
		if (p->state == SLEEPING && p->chan == chan)
			p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
	acquire(&ptable.lock);
	wakeup1(chan);
	release(&ptable.lock);
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
	struct proc *p;
	int havekids, pid;

	acquire(&ptable.lock);
	for (;;) {
		// Scan through table looking for exited children.
		havekids = 0;
		for (p = ptable.proc; p < &ptable[NPROC]; p++) {
			if (p->parent != proc)
				continue;
			havekids = 1;
			if (p->state == ZOMBIE) {
				//found one.
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
		if (!havekids || proc->killed) {
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(proc, &ptable.lock);
	}
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
	struct proc *p;
	int fd;
	if (proc == initproc)
		panic("init exiting");
	//close all open files
	for (fd = 0; fd < NOFILE; fd++) {
		if (proc->ofile[fd]) {
			fileclose(proc->ofile[fd]);
			proc->ofile[fd] = 0;
		}
	}
	begin_op();
	iput(proc->cwd);
	end_op();
	proc->cwd = 0;

	acquire(&ptable.lock);
	// Parent might be sleeping in wait().
	wakeup1(proc->parent);

	// Pass abandoned children to init.
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->parent == proc) {
			p->parent = initproc;
			if (p->state == ZOMBIE) {
				wakeup1(initproc);
			}
		}
	}

	// Jump into the scheduler, never to return.
	proc->state = ZOMBIE;
	sched();
	panic("zombie exit");
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
	struct proc *p;

	acquire(&ptable.lock);
	for(p=ptable.proc;p<&ptable.proc[NPROC];p++)
	{
		if(p->pid==pid)
		{
			p->killed=1;
		      // Wake process from sleep if necessary.
			if(p->state==SLEEPING)
				p->state=RUNNABLE;
			release(&ptable.lock);
			return 0;
		}
	}
	release(&ptable.lock);
	return -1;
}
