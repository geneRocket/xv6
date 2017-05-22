// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static struct{
	struct spinlock lock;
	int locking;
}cons;


void consoleinit()
{
	initlock(&cons.lock,"console");

	devsw[CONSOLE].write=consolewrite;
	devsw[CONSOLE].read=consoleread;
	cons.locking=1;

	picenable(IRQ_KBD);
	ioapicenable(IRQ_KBD,0);
}
