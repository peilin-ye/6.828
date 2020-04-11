	.text

/* Switch from current_thread to next_thread. Make next_thread
 * the current_thread, and set next_thread to 0.
 * Use eax as a temporary register; it is caller saved.
 */
	.globl thread_switch
thread_switch:
	/* we do whatever we want to eax */
	/* save old */
	pushal
	movl current_thread, %eax
	movl %esp, (%eax)

	/* switch stack */
	movl next_thread, %eax
	movl (%eax), %esp

	/* set current_thread to next_thread */
	movl %eax, current_thread
	
	/* set next_thread to 0 */
	movl $0, next_thread

	/* load new */
	popal
	ret
