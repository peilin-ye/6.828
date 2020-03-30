// Challenge 2.2
// Explicitly set, clear, or change the permissions of any mapping in the current address space.

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