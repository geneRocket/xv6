# The xv6 kernel starts executing in this file. This file is linked with
# the kernel C code, so it can refer to kernel symbols such as main().
# The boot block (bootasm.S and bootmain.c) jumps to entry below.


#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"


#此文件在逻辑高地址，物理低地址

# By convention, the _start symbol specifies the ELF entry point.
# Since we haven't set up virtual memory yet, our entry point is
# the physical address of 'entry'.

.globl _start
_start=V2P_WO(entry)


# Entering xv6 on boot processor, with paging off.
.globl entry
entry:
	 # Turn on page size extension for 4Mbyte pages
	movl %cr4,%eax
	orl $(CR4_PSE),%eax
	movl %eax,%cr4

	# Set page directory
	movl $(V2P_WO(entrypgdir)),%eax
	movl %eax,%cr3

	# Turn on paging
	movl %cr0,%eax
	orl     $(CR0_PG|CR0_WP), %eax
	#orl     $(CR0_PG), %eax
	#write protect
	movl    %eax, %cr0

	#All symbols have high addresses, including stack, so the stack will still be valid even
	#when the low mappings are removed.
	# Set up the stack pointer.
	movl $(stack+KSTACKSIZE),%esp

	# Jump to main(), and switch to executing at
	# high addresses. The indirect call is needed because
	# the assembler produces a PC-relative instruction
	# for a direct jump.

	mov $main,%eax
	jmp *%eax


.comm stack,KSTACKSIZE
