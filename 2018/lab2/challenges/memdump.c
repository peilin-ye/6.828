// Challenge 2.3
// Dump the contents of a range of memory given either a virtual or physical address range. Be sure the dump code behaves correctly when the range extends across page boundaries!

int
mon_memdump(int argc, char **argv, struct Trapframe *tf)
{
	// memdump -p -v lo upper
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