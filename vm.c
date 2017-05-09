#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[]; // defined by kernel.ld 数据段
pde_t * kpgdir; // for use in scheduler()

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t * walkpgdir(pde_t * pgdir, const void *va, int alloc) {
	pde_t *pde; //directory entry
	pte_t *pgtab; //page table地址
	//directory entry 放的是物理地址
	pde = &pgdir[PDX(va)];
	if (*pde & PTE_P) {
		pgtab = (pte_t *) P2V(PTE_ADDR(*pde));
	} else {
		if (!alloc || (pgtab = (pte_t *) kalloc()) == 0)
			return 0;
		// Make sure all those PTE_P bits are zero.
		memset(pgtab, 0, PGSIZE);
		// The permissions here are overly generous, but they can
		// be further restricted by the permissions in the page table
		// entries, if necessary.
		*pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
	}
	return &pgtab[PTX(va)];		//page table entry 的逻辑地址
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm) {
	char *a, *last;
	pte_t *pte;

	a = (char *) PGROUNDDOWN((uint )va);
	last = (char *) PGROUNDDOWN(((uint )va) + size - 1);
	for (;;) {
		if ((pte = walkpgdir(pgdir, a, 1)) == 0)
			return -1;
		if (*pte & PTE_P)
			panic("remap");
		*pte = pa | perm | PTE_P;
		if (a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;

	}
	return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
	void *virt;
	uint phys_start;
	uint phys_end;
	int perm;
} kmap[] = { { (void*) KERNBASE, 0, EXTMEM, PTE_W },		    // I/O space
		{ (void*) KERNLINK, V2P(KERNLINK), V2P(data), 0 },	// kern text+rodata
		{ (void*) data, V2P(data), PHYSTOP, PTE_W }, // kern data+memory
		{ (void*) DEVSPACE, DEVSPACE, 0, PTE_W }, // more devices
		};

// Set up kernel part of a page table.
pde_t * setupkvm(void) {
	pde_t *pgdir;
	struct kmap *k;

	if ((pgdir = (pde_t*) kalloc()) == 0)
		return 0;
	memset(pgdir, 0, PGSIZE);
	if (P2V(PHYSTOP) > (void*) DEVSPACE)
		panic("PHYSTOP too high");
	for (k = kmap; k < &kmap[NELEM(kmap)]; k++) {
		if (mappages(pgdir, k->virt, k->phys_end - k->phys_start,
				(uint) k->phys_start, k->perm) < 0)
			return 0;
	}
	return pgdir;
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void inituvm(pde_t *pgdir, char *init, uint sz) {
	char *mem;
	if (sz >= PGSIZE)
		panic("inituvm:more than a page");
	mem = kalloc();
	memset(mem, 0, PGSIZE);
	mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W | PTE_U);
	memmove(mem, init, sz);
}

// Switch TSS and h/w page table to correspond to process p.
void switchuvm(struct proc *p) {
	//instructs the hardware to execute system calls and interrupts on the process’s kernel stack.
	if (p == 0)
		panic("switchuvm: no process");
	if (p->kstack == 0)
		panic("switchuvm: no kstack");
	if (p->pgdir == 0)
		panic("switchuvm: no pgdir");

	pushcli();
	/*they are counted, so that it takes two calls to pop-
	 cli to undo two calls to pushcli; this way, if code holds two locks, interrupts will not
	 be reenabled until both locks have been released.*/
	cpu->gdt[SEG_TSS] = SEG16(STS_T32A, &cpu->ts, sizeof(cpu->ts) - 1, 0);
	cpu->gdt[SEG_TSS].s = 0;
	cpu->ts.ss0 = SEG_KDATA << 3;
	cpu->ts.esp0 = (uint) p->kstack + KSTACKSIZE;
	// setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
	// forbids I/O instructions (e.g., inb and outb) from user space
	cpu->ts.iomb=(ushort)0xFFFF;
	ltr(SEG_TSS<<3);
	lcr3(V2P(p->pgdir));// switch to process's address space
	popcli();

}


// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void kvmalloc(void)
{
	kpgdir=setupkvm();
	switchkvm();
}
