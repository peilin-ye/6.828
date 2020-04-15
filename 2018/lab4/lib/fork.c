// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	extern volatile pte_t uvpt[];
	int rc;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0)
		panic("pgfault: the faulting access was not a write!\n");
	if ((uvpt[(uintptr_t)addr >> PTXSHIFT] & PTE_COW) == 0)
		panic("pgfault: not a copy-on-write page!\n");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	envid_t eid = sys_getenvid();
	if ((rc = sys_page_alloc(eid, PFTEMP, (PTE_U|PTE_W))) < 0)
		panic("pgfault: sys_page_alloc() failed: %e\n", rc);

	memcpy((void *)PFTEMP, (void *)ROUNDDOWN(addr, PGSIZE), PGSIZE);
	if ((rc = sys_page_map(eid, (void *)PFTEMP, eid, (void *)ROUNDDOWN(addr, PGSIZE), (PTE_U|PTE_W))) < 0)
		panic("pgfault: sys_page_map() failed: %e\n", rc);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t ceid, unsigned pn)
{
	// LAB 4: Your code here.
	int rc;
	extern volatile pte_t uvpt[];
	envid_t peid = sys_getenvid();
	intptr_t va = (intptr_t)(pn * PGSIZE);

	if (uvpt[pn] & (PTE_COW|PTE_W)) {
		if ((rc = sys_page_map(peid, (void *)va, ceid, (void *)va, (PTE_COW|PTE_U))) < 0)
			return rc;
		if ((rc = sys_page_map(peid, (void *)va, peid, (void *)va, (PTE_COW|PTE_U))) < 0)
			return rc;
	} else {
		if ((rc = sys_page_map(peid, (void *)va, ceid, (void *)va, PTE_U)) < 0)
			return rc;
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int rc;

	set_pgfault_handler(pgfault);
	
	envid_t ceid = sys_exofork();	// child envid

	extern volatile pde_t uvpd[];
	extern volatile pte_t uvpt[];

	if (ceid < 0)
		return ceid;
	if (ceid == 0) {
		// we are the child.
		ceid = sys_getenvid();

		// update thisenv.
		thisenv = &envs[ENVX(ceid)];
		return 0;
	}

	// we call duppage() for our own user stack as well.
	// this will cause a page fault, as soon as we return from duppage().
	for (uintptr_t va = 0; va < UTOP;) {
		if ((uvpd[va >> PDXSHIFT] & PTE_P) == 0) {	// page table page not found.
			va += NPTENTRIES * PGSIZE;
			continue;
		}
		if ((uvpt[va >> PTXSHIFT] & PTE_P) == 0) {	// page table entry not found.
			va += PGSIZE;
			continue;
		}
		if (va == UXSTACKTOP - PGSIZE) {	// UXSTACKTOP is not remmaped!
			va += PGSIZE;
			continue;
		}
		// all other pages should be duppage()ed.
		// writable and COW pages willed be handled differently by duppage().
		if ((rc = duppage(ceid, (unsigned)(va/PGSIZE))) < 0)
			return rc;
		va += PGSIZE;
	}

	// let the parent set the exception stack for the child.
	// the child may immediately cause a page fault when it's run,
	// and if don't have a uxstack for the child by then, we're doomed.
	if ((rc = sys_page_alloc(ceid, (void *)(UXSTACKTOP-PGSIZE), (PTE_U|PTE_W))) < 0)
		return rc;

	// set up the page fault upcall for the child.
	// can't simply call set_pgfault_handler twice.
	extern void _pgfault_upcall(void);
	sys_env_set_pgfault_upcall(ceid, _pgfault_upcall);

	// marks the child RUNNABLE.
	sys_env_set_status(ceid, ENV_RUNNABLE);
	
	return ceid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
