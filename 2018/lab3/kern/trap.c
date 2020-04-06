#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/error.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	void vector0();
	SETGATE(idt[0], 1, GD_KT, vector0, 0);
	void vector1();
	SETGATE(idt[1], 1, GD_KT, vector1, 0);
	void vector2();
	SETGATE(idt[2], 1, GD_KT, vector2, 0);
	void vector3();
	// Exercise 6: Change to DPL_USER to make this work
	SETGATE(idt[3], 1, GD_KT, vector3, 3);
	void vector4();
	SETGATE(idt[4], 1, GD_KT, vector4, 0);
	void vector5();
	SETGATE(idt[5], 1, GD_KT, vector5, 0);
	void vector6();
	SETGATE(idt[6], 1, GD_KT, vector6, 0);
	void vector7();
	SETGATE(idt[7], 1, GD_KT, vector7, 0);
	void vector8();
	SETGATE(idt[8], 1, GD_KT, vector8, 0);

	void vector10();
	SETGATE(idt[10], 1, GD_KT, vector10, 0);
	void vector11();
	SETGATE(idt[11], 1, GD_KT, vector11, 0);
	void vector12();
	SETGATE(idt[12], 1, GD_KT, vector12, 0);
	void vector13();
	SETGATE(idt[13], 1, GD_KT, vector13, 0);
	void vector14();
	SETGATE(idt[14], 1, GD_KT, vector14, 0);

	void vector16();
	SETGATE(idt[16], 1, GD_KT, vector16, 0);
	void vector17();
	SETGATE(idt[17], 1, GD_KT, vector17, 0);
	void vector18();
	SETGATE(idt[18], 1, GD_KT, vector18, 0);
	void vector19();
	SETGATE(idt[19], 1, GD_KT, vector19, 0);

	void vector32();
	SETGATE(idt[32], 1, GD_KT, vector32, 0);
	void vector33();
	SETGATE(idt[33], 1, GD_KT, vector33, 0);
	void vector34();
	SETGATE(idt[34], 1, GD_KT, vector34, 0);
	void vector35();
	SETGATE(idt[35], 1, GD_KT, vector35, 0);
	void vector36();
	SETGATE(idt[36], 1, GD_KT, vector36, 0);
	void vector37();
	SETGATE(idt[37], 1, GD_KT, vector37, 0);

	void vector48();
	SETGATE(idt[48], 1, GD_KT, vector48, 3); // DPL_USER

	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;
	ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.

	switch (tf->tf_trapno) {
	case T_DEBUG:
		monitor(tf);
		break;
	case T_BRKPT:
		monitor(tf);
		break;
	case T_PGFLT:
		if ((tf->tf_cs & 3) == 0) {	// kernel mode
			panic("trap_dispatch: page fault happened in kernel mode!\n");
		}
		else {
			page_fault_handler(tf);
			break;
		}
	case T_SYSCALL:;	// a label can only be part of a statement and a declaration is not a statement
		uint32_t ret = syscall(tf->tf_regs.reg_eax,	\
							   tf->tf_regs.reg_edx,	\
							   tf->tf_regs.reg_ecx,	\
							   tf->tf_regs.reg_ebx,	\
							   tf->tf_regs.reg_edi,	\
							   tf->tf_regs.reg_esi);
		if (ret == -E_INVAL)
			panic("unknown syscall\n");

		tf->tf_regs.reg_eax = ret;
		break;

	// There are just so many types of exceptions and I don't feel like 
	// handling all of them this time. By default the user env is destroyed,
	// which is sufficient to pass part A. Will come back after better 
	// understanding these exceptions.
	default:
		// Unexpected trap: The user process or the kernel has a bug.
		print_trapframe(tf);
		if (tf->tf_cs == GD_KT)
			panic("unhandled trap in kernel\n");
		else {
			env_destroy(curenv);
			return;
		}
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		assert(curenv);

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// Return to the current environment, which should be running.
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

