#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#endif  // SOL >= 6

#include <kern/pci.h>

int e1000_attach(struct pci_func *pcif);
int tx_pkt(const char *buf, size_t nbytes);
int rx_pkt(char *buf);

// 3.2.3 Receive Descriptor Format
struct rd
{
    uint64_t address;
    uint16_t length;
    uint16_t pkt_cks;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};

// 3.3.3 Legacy Transmit Descriptor Format
struct td
{
    uint64_t address;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status; // RSV + STA
    uint8_t css;
    uint16_t special;
};

#define NTDRENTRIES     64      // Must be multiple of 8
#define NRDRENTRIES     128
#define DESC_BUF_SZ     2048    // 3.2.2 Receive Data Storage

// 3.2.3.1 Receive Descriptor Status Field
#define RDESC_STA_DD    0x01

// 3.3.3.1 Transmit Descriptor Command Field Format
#define TDESC_CMD_EOP   0x01
#define TDESC_CMD_RS    0x08

// 3.3.3.2 Transmit Descriptor Status Field Format
#define TDESC_STA_DD    0x01

// Table 5-1. Component Identification
#define PCI_E1000_VENDOR_ID     0x8086
#define PCI_E1000_DEVICE_ID     0x100e

// Table 13-2. Ethernet Controller Register Summary
#define RCTL_OFFSET     0x100

#define TCTL_OFFSET     0x400
#define TIPG_OFFSET     0x410

#define RDBAL_OFFSET    0x2800
#define RDBAH_OFFSET    0x2804
#define RDLEN_OFFSET    0x2808
#define RDH_OFFSET      0x2810
#define RDT_OFFSET      0x2818

#define TDBAL_OFFSET    0x3800
#define TDLEN_OFFSET    0x3808
#define TDH_OFFSET      0x3810
#define TDT_OFFSET      0x3818

#define MTA_OFFSET      0x5200
#define NMTAENTRIES     ((0x53FC - 0x5200) / 127)

#define RAL_0_OFFSET    0x5400
#define RAH_0_OFFSET    0x5404

// 13.4.2 Device Status Register
#define DSR_OFFSET      0x8

// 13.4.22 Receive Control Register
#define RCTL_EN             (1 << 1)
#define RCTL_LPE            (1 << 5)
#define RCTL_LBM_MASK       (0b11 << 6)
#define RCTL_BAM            (1 << 15)
#define RCTL_BSIZE_SHIFT    16
#define RCTL_SECRC          (1 << 26)

// 13.4.33 Transmit Control Register
#define TCTL_EN         0x2
#define TCTL_PSP        0x8

#define TCTL_COLD_SHIFT     12
#define TCTL_COLD_FDX       0x40

// 13.5.3 Receive Address High
#define RAH_AV      (1 << 31)

// Table 13-77. TIPG Register Bit Description
#define TIPG_IPGR           10      // IEEE 802.3 

#define TIPG_IPGR1_SHIFT    10
#define TIPG_IPGR1          4       // IEEE 802.3

#define TIPG_IPGR2_SHIFT    20
#define TIPG_IPGR2          6       // IEEE 802.3 

// 14.4 Receive Initialization
#define RDLEN_ALIGN_MASK    (128 - 1)

// 14.5 Transmit Initialization
#define TDLEN_ALIGN_MASK    (128 - 1)

// Misc
#define MAX_ETH_SZ      1518 