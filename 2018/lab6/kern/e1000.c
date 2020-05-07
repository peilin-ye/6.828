#include <inc/error.h>
#include <inc/string.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

volatile void *e1000;   // uint32_t

static void rx_init();
static void tx_init();

int pkt_count = 0;

// 3.4 Transmit Descriptor Ring Structure
__attribute__((__aligned__(16)))    // paragraph size
struct td tdr[NTDRENTRIES] = {0};
struct rd rdr[NRDRENTRIES] = {0};
uint32_t tdt = 0;     // Index into tdr[] and tx_pktbufs[].
uint32_t rdt = NRDRENTRIES - 1;

__attribute__((__aligned__(PGSIZE)))
char tx_pktbufs[NTDRENTRIES][DESC_BUF_SZ] = {0};
char rx_pktbufs[NRDRENTRIES][DESC_BUF_SZ] = {0};

static void
rx_init(void)
{
    // initialize rdr
    for (int i = 0; i < NRDRENTRIES; i++)
        rdr[i].address = PADDR(&rx_pktbufs[i]);

    // 14.4 Receive Initialization
    *(uint32_t *)(e1000 + RAL_0_OFFSET) = 0x12005452;       // QEMU's default MAC address:
    *(uint32_t *)(e1000 + RAH_0_OFFSET) = 0x5634 | RAH_AV;  // 52:54:00:12:34:56

    for (int i = 0; i < NMTAENTRIES; i++)
        ((uint32_t *)(e1000 + MTA_OFFSET))[i] = 0;

    *(uint32_t *)(e1000 + RDBAL_OFFSET) = PADDR(rdr);

    if ((sizeof(rdr) & RDLEN_ALIGN_MASK) != 0)
        panic("RDLEN must be 128-byte aligned!\n");
    *(uint32_t *)(e1000 + RDLEN_OFFSET) = sizeof(rdr);

    *(uint32_t *)(e1000 + RDH_OFFSET) = 0;
    *(uint32_t *)(e1000 + RDT_OFFSET) = NRDRENTRIES - 1;

    uint32_t rctl = RCTL_SECRC | DESC_BUF_SZ << RCTL_BSIZE_SHIFT | RCTL_BAM | RCTL_EN;
    rctl &= ~RCTL_LPE;
    *(uint32_t *)(e1000 + RCTL_OFFSET) = rctl;
}

static void
tx_init(void)
{
    // initialize tdr
    for (int i = 0; i < NTDRENTRIES; i++)
        tdr[i].address = PADDR(&tx_pktbufs[i]);
    
    // 14.5 Transmit Initialization
    *(uint32_t *)(e1000 + TDBAL_OFFSET) = PADDR(tdr);

    if ((sizeof(tdr) & TDLEN_ALIGN_MASK) != 0)
        panic("TDLEN must be 128-byte aligned!\n");
    *(uint32_t *)(e1000 + TDLEN_OFFSET) = sizeof(tdr);

    *(uint32_t *)(e1000 + TDH_OFFSET) = 0;
    *(uint32_t *)(e1000 + TDT_OFFSET) = 0;

    // TCTL.CT is ignored, since we assume full-duplex operation.
    *(uint32_t *)(e1000 + TCTL_OFFSET) = (TCTL_COLD_FDX << TCTL_COLD_SHIFT) | TCTL_PSP | TCTL_EN;

    *(uint32_t *)(e1000 + TIPG_OFFSET) = (TIPG_IPGR2 << TIPG_IPGR2_SHIFT | \
                                          TIPG_IPGR1 << TIPG_IPGR1_SHIFT | \
                                          TIPG_IPGR);
}

int
e1000_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);

    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	cprintf("Device Status Register: 0x%x\n", *(uint32_t *)(e1000 + DSR_OFFSET));
    
    rx_init();
    tx_init();
    return 0;
}

int
tx_pkt(const char *buf, size_t nbytes)
{
    if (nbytes > DESC_BUF_SZ)
        panic("tx_pkt: invalid packet size!\n");

    if ((tdr[tdt].cmd & TDESC_CMD_RS) != 0) {         // If this descriptor has been used before. We always set RS when transmitting a packet.
        if ((tdr[tdt].status & TDESC_STA_DD) == 0)    // Still in use!
            return -E_TX_FULL;
    }
    // Copy data into packet buffer
    memcpy(&tx_pktbufs[tdt], buf, nbytes);
    tdr[tdt].length = (uint16_t)nbytes;
    tdr[tdt].cmd |= TDESC_CMD_RS | TDESC_CMD_EOP;

    tdt = (tdt + 1) % NTDRENTRIES;     // It's a ring
    *(uint32_t *)(e1000 + TDT_OFFSET) = tdt;
    return 0;
}

// return a length
int
rx_pkt(char *buf)
{
    uint32_t next = (rdt + 1) % NRDRENTRIES;
    if ((rdr[next].status & RDESC_STA_DD) == 0)
        return -E_RX_EMPTY;
    
    rdt = next;
    pkt_count++;
    // Copy data out of packet buffer
    uint16_t length = rdr[rdt].length;
    memcpy(buf, &rx_pktbufs[rdt], length);
    
    rdr[next].status &= ~RDESC_STA_DD;
    *(uint32_t *)(e1000 + RDT_OFFSET) = rdt;
    return length;
}