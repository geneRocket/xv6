



struct context{
	uint edi;
	uint esi;
	uint ebx;
	uint ebp;
	uint eip;
};

enum procstate{UNUSED,EMBRYO,SLEEPING,RUNNABLE,RUNNING,ZOMBIE};

struct proc{
	uint sz; // Size of process memory (bytes)
	pde_t*pgdir; // Page table
	char *kstack;// Bottom of kernel stack for this process
	enum procstate state;        // Process state
	int pid;                     // Process ID
	struct proc *parent;// Parent process
	struct trapframe *tf;// Trap frame for current syscall
	struct context *context;// swtch() here to run process
	void *chan;// If non-zero, sleeping on chan
	int killed;// If non-zero, have been killed
	struct file *ofile[NOFILE];// Open files
	struct inode *cwd; // Current directory
	char name[16]; // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
