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
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/time.h>

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
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	// Lab 4: Do you really understand how to use SETGATE()?
	// In JOS, interruption is always disabled when we are in the kernel.
	// How to make sure? By setting is_trap to 0, we tell the CPU
	// to clear FL_IF.
	// Then what about dpl? If it's 0, a user can't invoke it explicitly
	// using an int instruction.
	void vector0();
	SETGATE(idt[0], 0, GD_KT, vector0, 0);
	void vector1();
	SETGATE(idt[1], 0, GD_KT, vector1, 0);
	void vector2();
	SETGATE(idt[2], 0, GD_KT, vector2, 0);
	void vector3();
	// Exercise 6: Change to DPL_USER to make this work
	SETGATE(idt[3], 0, GD_KT, vector3, 3);	// T_BRKPT
	void vector4();
	SETGATE(idt[4], 0, GD_KT, vector4, 0);
	void vector5();
	SETGATE(idt[5], 0, GD_KT, vector5, 0);
	void vector6();
	SETGATE(idt[6], 0, GD_KT, vector6, 0);
	void vector7();
	SETGATE(idt[7], 0, GD_KT, vector7, 0);
	void vector8();
	SETGATE(idt[8], 0, GD_KT, vector8, 0);

	void vector10();
	SETGATE(idt[10], 0, GD_KT, vector10, 0);
	void vector11();
	SETGATE(idt[11], 0, GD_KT, vector11, 0);
	void vector12();
	SETGATE(idt[12], 0, GD_KT, vector12, 0);
	void vector13();
	SETGATE(idt[13], 0, GD_KT, vector13, 0);
	void vector14();
	SETGATE(idt[14], 0, GD_KT, vector14, 0);

	void vector16();
	SETGATE(idt[16], 0, GD_KT, vector16, 0);
	void vector17();
	SETGATE(idt[17], 0, GD_KT, vector17, 0);
	void vector18();
	SETGATE(idt[18], 0, GD_KT, vector18, 0);
	void vector19();
	SETGATE(idt[19], 0, GD_KT, vector19, 0);

	void vector32();
	SETGATE(idt[32], 0, GD_KT, vector32, 0);	// IRQ_TIMER
	void vector33();
	SETGATE(idt[33], 0, GD_KT, vector33, 0);	// IRQ_KBD
	void vector34();
	SETGATE(idt[34], 0, GD_KT, vector34, 0);
	void vector35();
	SETGATE(idt[35], 0, GD_KT, vector35, 0);
	void vector36();
	SETGATE(idt[36], 0, GD_KT, vector36, 0);	// IRQ_SERIAL
	void vector37();
	SETGATE(idt[37], 0, GD_KT, vector37, 0);
	void vector38();
	SETGATE(idt[38], 0, GD_KT, vector38, 0);
	void vector39();
	SETGATE(idt[39], 0, GD_KT, vector39, 0);	// IRQ_SPURIOUS
	void vector40();
	SETGATE(idt[40], 0, GD_KT, vector40, 0);
	void vector41();
	SETGATE(idt[41], 0, GD_KT, vector41, 0);
	void vector42();
	SETGATE(idt[42], 0, GD_KT, vector42, 0);
	void vector43();
	SETGATE(idt[43], 0, GD_KT, vector43, 0);
	void vector44();
	SETGATE(idt[44], 0, GD_KT, vector44, 0);
	void vector45();
	SETGATE(idt[45], 0, GD_KT, vector45, 0);
	void vector46();
	SETGATE(idt[46], 0, GD_KT, vector46, 0);	// IRQ_IDE
	void vector47();
	SETGATE(idt[47], 0, GD_KT, vector47, 0);

	// In xv6, FL_IF is not cleared when a syscall is made.
	// In JOS, we don't even allow that. No interrupts in kernel!
	void vector48();
	SETGATE(idt[48], 0, GD_KT, vector48, 3); 	// T_SYSCALL, DPL_USER

	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//   - Initialize cpu_ts.ts_iomb to prevent unauthorized environments
	//     from doing IO (0 is not the correct value!)
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	thiscpu->cpu_ts.ts_esp0 = (uintptr_t) percpu_kstacks[thiscpu->cpu_id];
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[(GD_TSS0 >> 3) + thiscpu->cpu_id] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + thiscpu->cpu_id].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0 + (thiscpu->cpu_id * sizeof(struct Gatedesc)));

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
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
		page_fault_handler(tf);
		break;

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.

	case IRQ_OFFSET + IRQ_TIMER:
		lapic_eoi();

		// Add time tick increment to clock interrupts.
		// Be careful! In multiprocessors, clock interrupts are
		// triggered on every CPU.
		// LAB 6: Your code here.
		if (thiscpu == bootcpu)
			time_tick();

		sched_yield();
		// shouldn't reach here...
		panic("trap_dispatch: IRQ_TIMER: sched_yield() returned!\n");
	
	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.
	case IRQ_OFFSET + IRQ_KBD:
		kbd_intr();
		break;
	
	case IRQ_OFFSET + IRQ_SERIAL:
		serial_intr();
		break;

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	case IRQ_OFFSET + IRQ_SPURIOUS:
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		break;

	case T_SYSCALL:;	// a label can only be part of a statement and a declaration is not a statement
		uint32_t ret = syscall(tf->tf_regs.reg_eax,	\
							   tf->tf_regs.reg_edx,	\
							   tf->tf_regs.reg_ecx,	\
							   tf->tf_regs.reg_ebx,	\
							   tf->tf_regs.reg_edi,	\
							   tf->tf_regs.reg_esi);
		// Lab 4: Why use -E_INVAL? A lot of syscalls generate -E_INVALs and they
		// obviously have other meanings...
		if (ret == -E_UNSPECIFIED)
			panic("trap_dispatch: unknown syscall\n");
		if (ret == -E_INVAL) {	
			panic("trap_dispatch: syscall() returned -E_INVAL\n");
		}
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

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		assert(curenv);
		lock_kernel();

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

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

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	if ((tf->tf_cs & 3) == 0) {	// kernel mode
		panic("trap_dispatch: page fault happened in kernel mode!\n");
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// It is convenient for our code which returns from a page fault
	// (lib/pfentry.S) to have one word of scratch space at the top of the
	// trap-time stack; it allows us to more easily restore the eip/esp. In
	// the non-recursive case, we don't have to worry about this because
	// the top of the regular user stack is free.  In the recursive case,
	// this means we have to leave an extra word between the current top of
	// the exception stack and the new stack frame because the exception
	// stack _is_ the trap-time stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.
	if (!curenv->env_pgfault_upcall)
		goto bad;
	
	int recursive = (tf->tf_esp >= UXSTACKTOP - PGSIZE) && (tf->tf_esp < UXSTACKTOP);
	struct UTrapframe *utf;

	if (recursive) {
		utf = (struct UTrapframe*)(tf->tf_esp - sizeof(uint32_t) - sizeof(struct UTrapframe));
		user_mem_assert(curenv, (void *)utf, tf->tf_esp - (uintptr_t)utf, PTE_W);
	} else {
		utf = (struct UTrapframe*)(UXSTACKTOP - sizeof(struct UTrapframe));
		user_mem_assert(curenv, (void *)utf, UXSTACKTOP - (uintptr_t)utf, PTE_W);
	}

	utf->utf_esp = tf->tf_esp;	// Yes we will push the return address onto trap-time stack,
								// so this value need to be reduced by 4 before being popped 
								// into %esp later in _pgfault_upcall... But we will do it in
								// _pgfault_upcall.
	utf->utf_eflags = tf->tf_eflags;
	utf->utf_eip = tf->tf_eip;
	utf->utf_regs = tf->tf_regs;
	utf->utf_err = tf->tf_err;
	utf->utf_fault_va = fault_va;

	// Prepare to call env_run()
	tf->tf_eip = (uintptr_t)curenv->env_pgfault_upcall;
	tf->tf_esp = (uintptr_t)utf;
	env_run(curenv);

	bad:
		// Destroy the environment that caused the fault.
		cprintf("[%08x] user fault va %08x ip %08x\n",
			curenv->env_id, fault_va, tf->tf_eip);
		print_trapframe(tf);
		env_destroy(curenv);
}