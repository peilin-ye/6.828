// Challenge 2.1

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
			cprintf("[UNMAPPED]");
		}
		
		cprintf("\n");
		if (p >= 0xfffff000)
			break;
	}

	return 0;
}