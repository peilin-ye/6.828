#include <inc/error.h>
#include <inc/string.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

volatile void *e1000;

static void rx_init();
static void tx_init();

int pkt_count = 0;

/* 3.4 Transmit Descriptor Ring Structure */
__attribute__((__aligned__(16)))    /* paragraph size */
struct td tdr[NTDRENTRIES] = {0};
struct rd rdr[NRDRENTRIES] = {0};
uint32_t tdt = 0;     /* Index into tdr and tx_pktbufs */
uint32_t rdt = NRDRENTRIES - 1;

__attribute__((__aligned__(PGSIZE)))
char tx_pktbufs[NTDRENTRIES][DESC_BUF_SZ] = {0};
char rx_pktbufs[NRDRENTRIES][DESC_BUF_SZ] = {0};

/**
 * rx_init - initialize Receive Descriptor Ring (RDR)
 **/
static void
rx_init(void)
{
    for (int i = 0; i < NRDRENTRIES; i++)
        rdr[i].address = PADDR(&rx_pktbufs[i]);

    /* 14.4 Receive Initialization */
    *(uint32_t *)(e1000 + RAL_0_OFFSET) = 0x12005452;       /* QEMU's default MAC address: 52:54:00:12:34:56 */
    *(uint32_t *)(e1000 + RAH_0_OFFSET) = 0x5634 | RAH_AV;

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

/**
 * tx_init - initialize Transmit Descriptor Ring (TDR)
 **/
static void
tx_init(void)
{
    for (int i = 0; i < NTDRENTRIES; i++)
        tdr[i].address = PADDR(&tx_pktbufs[i]);
    
    /* 14.5 Transmit Initialization */
    *(uint32_t *)(e1000 + TDBAL_OFFSET) = PADDR(tdr);

    if ((sizeof(tdr) & TDLEN_ALIGN_MASK) != 0)
        panic("TDLEN must be 128-byte aligned!\n");
    *(uint32_t *)(e1000 + TDLEN_OFFSET) = sizeof(tdr);

    *(uint32_t *)(e1000 + TDH_OFFSET) = 0;
    *(uint32_t *)(e1000 + TDT_OFFSET) = 0;

    /* TCTL.CT is ignored, since we assume full-duplex operation */
    *(uint32_t *)(e1000 + TCTL_OFFSET) = (TCTL_COLD_FDX << TCTL_COLD_SHIFT) | TCTL_PSP | TCTL_EN;

    *(uint32_t *)(e1000 + TIPG_OFFSET) = (TIPG_IPGR2 << TIPG_IPGR2_SHIFT | \
                                          TIPG_IPGR1 << TIPG_IPGR1_SHIFT | \
                                          TIPG_IPGR);
}

/**
 * e1000_attach - enable, set up MMIO region for, and initialize e1000
 * @pcif: PCI function struct
 *
 * Returns zero on success.
 **/
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

/**
 * tx_pkt - transmit a packet to Transmit Descriptor Ring (TDR)
 * @buf: buffer to read packet data from
 * @nbytes: size of the packet (in bytes)
 * 
 * Returns zero on success, or -E_TX_FULL if TDR is full.
 **/
int
tx_pkt(const char *buf, size_t nbytes)
{
    if (nbytes > DESC_BUF_SZ)
        panic("tx_pkt: invalid packet size!\n");

    /* If this descriptor has been used before */
    if ((tdr[tdt].cmd & TDESC_CMD_RS) != 0) {
	if ((tdr[tdt].status & TDESC_STA_DD) == 0)    /* Still in use */
            return -E_TX_FULL;
    }
    /* Copy data into packet buffer */
    memcpy(&tx_pktbufs[tdt], buf, nbytes);
    tdr[tdt].length = (uint16_t)nbytes;
    tdr[tdt].cmd |= TDESC_CMD_RS | TDESC_CMD_EOP;

    /* TDR is a ring */
    tdt = (tdt + 1) % NTDRENTRIES;
    *(uint32_t *)(e1000 + TDT_OFFSET) = tdt;
    return 0;
}

/**
 * rx_pkt - receive a packet from Receive Descriptor Ring (RDR)
 * @buf: buffer to write packet data to
 * 
 * Returns length of the packet, or -E_RX_EMPTY if RDR is empty.
 **/
rx_pkt(char *buf)
{
    /* RDR is a ring */
    uint32_t next = (rdt + 1) % NRDRENTRIES;
    if ((rdr[next].status & RDESC_STA_DD) == 0)
        return -E_RX_EMPTY;
    
    rdt = next;
    pkt_count++;
    
    /* Copy data out of packet buffer */
    uint16_t length = rdr[rdt].length;
    memcpy(buf, &rx_pktbufs[rdt], length);
    
    rdr[next].status &= ~RDESC_STA_DD;
    *(uint32_t *)(e1000 + RDT_OFFSET) = rdt;
    return length;
}
