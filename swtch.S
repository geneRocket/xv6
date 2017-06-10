#   void swtch(struct context **old, struct context *new);

# Save current register context in old
# and then load register context from new.

//change eip and esp
.globl swtch
swtch:
	//must do this before it changes the stack pointer and can no longer access the arguments via %esp
	movl 4(%esp),%eax
	movl 8(%esp),%edx
	//scheduler
	# Save old callee-save registers
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi

	# Switch stacks
	movl %esp,(%eax)
	movl %edx,%esp

	# Load new callee-save registers
	popl %edi
	popl %esi
	popl %ebx
 	popl %ebp

	ret
