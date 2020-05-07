#include "ns.h"

uint32_t pkt_count = 0;

__attribute__((__aligned__(PGSIZE)))
union Nsipc nsipcbufs[2];

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	int r;
	int8_t s = 0; // nsipcbufs selector
	char tmpbuf[NS_BUFSIZE] = {0};

	// LAB 6: Your code here:
	for (;;) {
		// 	- read a packet from the device driver
		while ((r = sys_rx_pkt(tmpbuf)) < 0);
		pkt_count++;

		//	- send it to the network server
		// Hint: When you IPC a page to the network server, it will be
		// reading from it for a while, so don't immediately receive
		// another packet in to the same physical page.
		nsipcbufs[s].pkt.jp_len = r;
		memcpy(nsipcbufs[s].pkt.jp_data, tmpbuf, r);
		
		ipc_send(ns_envid, NSREQ_INPUT, (void *)&nsipcbufs[s], PTE_U|PTE_P);
		s ^= 1;
	}
}
