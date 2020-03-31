// Challenge 2.4
// Display physical address and number of references for all in-use page frames

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