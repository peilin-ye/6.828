// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a listing of function call frames", mon_backtrace },
	{ "showmappings", "Display all of the physical page mappings (or lack thereof) that apply to a particular range of virtual/linear addresses in the currently active address space.", mon_showmappings },
	{ "chpgperm", "Explicitly set, clear, or change the permissions of any mapping in the current address space", mon_chpgperm},
	{ "memdump", "Dump the contents of a range of memory given either a virtual or physical address range.", mon_memdump},
	{ "showpg", "Display useful information of physical pages", mon_showpg},
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t *ebp;
	struct Eipdebuginfo info;

	ebp = (uint32_t *)read_ebp();

	cprintf("Stack backtrace:\n");
	while (1) {
		cprintf("  ebp %08x", ebp);
		cprintf("  eip %08x", *(ebp+1));
		cprintf("  args %08x %08x %08x %08x %08x\n", *(ebp+2), *(ebp+3), *(ebp+4), *(ebp+5), *(ebp+6));

		if (!debuginfo_eip((uintptr_t)*(ebp+1), &info)) {
			cprintf("         %s:%d:", info.eip_file, info.eip_line);
			cprintf(" %.*s", info.eip_fn_namelen, info.eip_fn_name);
			cprintf("+%d\n", (*(ebp+1) - info.eip_fn_addr));
		}

		ebp = (uint32_t *)(*ebp);
		if (ebp == 0)
			break;
	}
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if ((argc < 2) | (argc > 3)) {
		cprintf("Usage: showmappings LOWERBOUND [UPPERBOUND]\n");
		return 0;
	}

	// Args are not sufficiently sanitized. Use with caution!
	uintptr_t lo = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	uintptr_t up = (argc == 2) ? lo : ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE);

	if (lo > up) {
		cprintf("showmappings: UPPERBOUND cannot be lower than LOWERBOUND\n");
		return 0;
	}

	pte_t *pte;
	extern pde_t *kern_pgdir;
	char flags[] = "GSDACTUWP"; 

	cprintf("Virtual Address      Physical Address      Flags\n");

	for (uintptr_t p = lo; p <= up; p += PGSIZE) {

		cprintf("0x%08x           ", p);

		pte = pgdir_walk(kern_pgdir, (void *)p, 0);
		if (pte) {
			cprintf("0x%08x            ", PTE_ADDR(*pte));
			int flag = (*pte & 0xFFF);
			for (int i = 0; i < 9; i++) {
				if ((flag >> (8 - i)) & 1) {
					cprintf("%c", flags[i]);
				} else {
					cprintf("-");
				}
			}
		} else {
			cprintf("(UNMAPPED)");
		}
		
		cprintf("\n");
		if (p >= 0xfffff000)
			break;
	}

	return 0;
}

int
mon_chpgperm(int argc, char **argv, struct Trapframe *tf)
{
	int action = 0;
	#define F_SET    1
	#define F_CLEAR  2
	#define F_CHANGE 3
	
	if (argc < 3) {
		cprintf("Usage: chpgperm ACTION VADDR [MODE]\n");
		cprintf("ACTION is one of \"set\", \"clear\" or \"change\".\n");
		return 0;
	}

	if (!(strcmp(argv[1], "set")))
		action = F_SET;
	else if (!(strcmp(argv[1], "clear")))
		action = F_CLEAR;
	else if (!(strcmp(argv[1], "change")))
		action = F_CHANGE;
	else {
		cprintf("chpgperm: Invalid ACTION!\n");
		return 0;
	}

	// Args are not sufficiently sanitized. Use with caution!
	extern pde_t *kern_pgdir;
	uintptr_t va = ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE);
	pte_t *pte = pgdir_walk(kern_pgdir, (void *)va, 0);

	if (!pte) {
		cprintf("chpgperm: Cannot change permission for page [0x%08x - 0x%08x): Corresponding page table page does not exist!\n", va, (va+PGSIZE));
		return 0;
	}
	if (!(*pte & PTE_P)) {
		cprintf("chpgperm: Cannot change permission for page [0x%08x - 0x%08x): Corresponding page table entry does not exist!\n", va, (va+PGSIZE));
		return 0;
	}

	if (action == F_SET) {
		if (argc != 4) {
			cprintf("Usage: chpgperm set VADDR MODE\n");
			cprintf("Each MODE is of the form '([[Ss]|[Uu]])([[Rr]|[Ww]])'.\n");
			return 0;
		}
		if (strlen(argv[3]) != 2) {
			cprintf("chpgperm set: Each MODE is of the form '([[Ss]|[Uu]])([[Rr]|[Ww]])'.\n");
			return 0;
		}
		
		int perm = 0;
		if ((argv[3][0] == 'U') | (argv[3][0] == 'u')) {
			perm |= PTE_U;
		}
		else if ((argv[3][0] != 'S') & (argv[3][0] != 's')) {
			cprintf("chpgperm set: '%c' is not a valid flag for the U/S bit.\n", argv[3][0]);
			return 0;
		}

		if ((argv[3][1] == 'W') | (argv[3][1] == 'w')) {
			perm |= PTE_W;
		}
		else if ((argv[3][1] != 'R') & (argv[3][1] != 'r')) {
			cprintf("chpgperm set: '%c' is not a valid flag for the R/W bit.\n", argv[3][1]);
			return 0;			
		}
		
		*pte = (*pte & ~(PTE_U | PTE_W)) | perm;
		cprintf("[0x%08x - 0x%08x): ", va, (va+PGSIZE));
		cprintf((*pte & PTE_U) ? "User" : "Supervisor");
		cprintf(" | ");
		cprintf((*pte & PTE_W) ? "Read/write" : "Read-only");
		cprintf("\n");
	}

	if (action == F_CLEAR) {
		if (argc != 3) {
			cprintf("chpgperm clear: Too many arguments!\n");
			cprintf("Usage: chpgperm clear VADDR\n");
			return 0;
		}
		
		*pte = *pte & ~(PTE_U | PTE_W);
		cprintf("[0x%08x - 0x%08x): Supervisor | Read-only\n", va, (va+PGSIZE));
	}
	
	if (action == F_CHANGE) {
		if (argc != 4) {
			cprintf("Usage: chpgperm change VADDR MODE\n");
			cprintf("Each MODE is of the form '[+-]([Uu]|[Ww])'.\n");
			return 0;
		}

		// Changing two flags at the same time is not allowed.
		if (strlen(argv[3]) != 2) {
			cprintf("chpgperm change: Each MODE is of the form '[+-]([Uu]|[Ww])'.\n");
			return 0;
		}

		int add = 0, perm = 0;
		if (argv[3][0] == '+') {
			add = 1;
		} else if (argv[3][0] == '-') {
			add = 0;
		} else {
			cprintf("chpgperm change: Each MODE is of the form '[+-]([Uu]|[Ww])'.\n");
			return 0;
		}

		if ((argv[3][1] == 'U') | (argv[3][1] == 'u')) {
			perm = PTE_U;
		} else if ((argv[3][1] == 'W') | (argv[3][1] == 'w')) {
			perm = PTE_W;
		} else {
			cprintf("chpgperm change: '%c' is not a valid flag either for the U/S bit or the R/W bit.\n", argv[3][1]);
			return 0;
		}

		if (add) {
			*pte |= perm;
		} else {
			*pte &= (~perm);
		}

		cprintf("[0x%08x - 0x%08x): ", va, (va+PGSIZE));
		cprintf((*pte & PTE_U) ? "User" : "Supervisor");
		cprintf(" | ");
		cprintf((*pte & PTE_W) ? "Read/write" : "Read-only");
		cprintf("\n");
	}

	return 0;
}

int
mon_memdump(int argc, char **argv, struct Trapframe *tf)
{
	if ((argc < 3) | (argc > 4)) {
		cprintf("Usage: memdump OPTION LOWERBOUND [UPPERBOUND]\n");
		return 0;
	}

	uintptr_t lo = ROUNDDOWN(strtol(argv[2], NULL, 16), 0x10);
	uintptr_t up = (argc == 3) ? lo : ROUNDDOWN(strtol(argv[3], NULL, 16), 0x10);
	extern pde_t *kern_pgdir;

	if (lo > up) {
		cprintf("memdump: UPPERBOUND cannot be lower than LOWERBOUND\n");
		return 0;
	}

	int virtual = 0;
	if ((!(strcmp(argv[1], "-V"))) | (!(strcmp(argv[1], "-v")))) {
		virtual = 1;
	} else if ((!(strcmp(argv[1], "-P"))) | (!(strcmp(argv[1], "-p")))) {
		virtual = 0;
	} else {
		cprintf("memdump: OPTION is of the form '(-[Vv])|(-[Pp])'.\n");
		return 0;
	}

	cprintf("kern_pgdir: 0x%08x\n", (uintptr_t) kern_pgdir);

	if (!virtual) {
		cprintf("Physical\n");
		for (uintptr_t p = lo; p <= up; p += 0x10) {
			cprintf("[0x%08x]    ", p);
			for (int j = 0; j < 0x10; j += 4) {
				cprintf("%08lx ", *(long *)KADDR(p + j));
			}
			cprintf("\n");
		}
	} else {
		cprintf("Virtual       Physical\n");
		for (uintptr_t v = lo; v <= up; v += 0x10) {
			cprintf("[0x%08x]  ", v);
			struct PageInfo *pp = page_lookup(kern_pgdir, (void *)v, NULL);
			if (!pp) {
				cprintf("(UNMAPPED)\n");
				continue;
			} else {
				cprintf("[0x%08x]    ", page2pa(pp) + PGOFF(v));
				for (int j = 0; j < 0x10; j += 4) {
					cprintf("%08lx ", *(long *)(v + j));
				}
				cprintf("\n");
				continue;
			}
		}		
	}
	return 0;
}

int
mon_showpg(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 1) {
		cprintf("Usage: showpg\n");
		return 0;
	}
	
	extern struct PageInfo *pages;
	extern size_t npages;

	for (int i = 0; i < npages; i++) {
		if (pages[i].pp_ref) {
			cprintf("[0x%08x] reference(s): %d\n", page2pa(pages+i), pages[i].pp_ref);
		}
	}

	return 0;	
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
