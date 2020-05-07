#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	for (;;) {
		int r;
		envid_t from_env_store;

		// 	- read a packet from the network server
		if ((r = ipc_recv(&from_env_store, (void *)&nsipcbuf, 0)) < 0)
			panic("output: ipc_recv() failed!\n");
		if (from_env_store != ns_envid)
			panic("output: unexpected IPC sender!\n");
		if (r != NSREQ_OUTPUT)
			panic("output: unexpected IPC type!\n");

		//	- send the packet to the device driver
		sys_tx_pkt(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
	}
}
