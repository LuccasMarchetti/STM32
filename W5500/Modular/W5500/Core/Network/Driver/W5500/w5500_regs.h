/*
 * w5500_regs.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System (extracted from original w5500.h)
 *
 * Definições de todos os registradores do W5500
 * Este arquivo contém apenas #defines - sem lógica ou dependências
 */

#ifndef NETWORK_DRIVER_W5500_W5500_REGS_H_
#define NETWORK_DRIVER_W5500_W5500_REGS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =============================================================================
 * W5500 ADDRESSING
 * ============================================================================= */

// Common register block select bits
#define W5500_BSB_COMMON     0x00

// Socket registers base addresses (n = socket number 0-7)
#define W5500_BSB_SOCKET_REG(n)   (((n) * 4 + 0x01) << 3)
#define W5500_BSB_SOCKET_TX(n)    (((n) * 4 + 0x02) << 3)
#define W5500_BSB_SOCKET_RX(n)    (((n) * 4 + 0x03) << 3)

// Read/Write bit definitions
#define W5500_RWB_READ    0x00
#define W5500_RWB_WRITE   0x04

// Data length mode definitions
#define W5500_OPCODE_VDM  0x00
#define W5500_OPCODE_FDM1 0x01
#define W5500_OPCODE_FDM2 0x02
#define W5500_OPCODE_FDM4 0x03

/* =============================================================================
 * COMMON REGISTERS
 * ============================================================================= */

// Mode Register
#define W5500_COMMON_MR                 0x0000
#define W5500_COMMON_MR_RST             ((uint8_t)(1u << 7))
#define W5500_COMMON_MR_WOL             ((uint8_t)(1u << 5))
#define W5500_COMMON_MR_PB              ((uint8_t)(1u << 4))
#define W5500_COMMON_MR_PPPoE           ((uint8_t)(1u << 3))
#define W5500_COMMON_MR_FARP            ((uint8_t)(1u << 1))

// Gateway Address Register
#define W5500_COMMON_GAR                0x0001
#define W5500_COMMON_GAR_LEN            4

// Subnet Mask Register
#define W5500_COMMON_SUBR               0x0005
#define W5500_COMMON_SUBR_LEN           4

// Source Hardware Address Register (MAC)
#define W5500_COMMON_SHAR               0x0009
#define W5500_COMMON_SHAR_LEN           6

// Source IP Address Register
#define W5500_COMMON_SIPR               0x000F
#define W5500_COMMON_SIPR_LEN           4

// Interrupt Level Register
#define W5500_COMMON_INTLEVEL           0x0013
#define W5500_COMMON_INTLEVEL_LEN       2

// Interrupt Register
#define W5500_COMMON_IR                 0x0015
#define W5500_COMMON_IR_CONFLICT        ((uint8_t)(1u << 7))
#define W5500_COMMON_IR_UNREACH         ((uint8_t)(1u << 6))
#define W5500_COMMON_IR_PPPoE           ((uint8_t)(1u << 5))
#define W5500_COMMON_IR_MP              ((uint8_t)(1u << 4))

// Interrupt Mask Register
#define W5500_COMMON_IMR                0x0016
#define W5500_COMMON_IMR_CONFLICT       ((uint8_t)(1u << 7))
#define W5500_COMMON_IMR_UNREACH        ((uint8_t)(1u << 6))
#define W5500_COMMON_IMR_PPPoE          ((uint8_t)(1u << 5))
#define W5500_COMMON_IMR_MP             ((uint8_t)(1u << 4))

// Socket Interrupt Register
#define W5500_COMMON_SIR                0x0017

// Socket Interrupt Mask Register
#define W5500_COMMON_SIMR               0x0018

// Retry Time Register
#define W5500_COMMON_RTR                0x0019
#define W5500_COMMON_RTR_LEN            2
#define W5500_COMMON_RTR_us             100
#define W5500_COMMON_RTR_DEFAULT        0x07D0

// Retry Count Register
#define W5500_COMMON_RCR                0x001B
#define W5500_COMMON_RCR_LEN            1
#define W5500_COMMON_RCR_DEFAULT        8

// PPP LCP Request Timer Register
#define W5500_COMMON_PTIMER             0x001C
#define W5500_COMMON_PTIMER_ms          25
#define W5500_COMMON_PTIMER_DEFAULT     0x0028

// PPP LCP Magic Number Register
#define W5500_COMMON_PMAGIC             0x001D

// Physical Address Register in PPPoE mode
#define W5500_COMMON_PHAR               0x001E
#define W5500_COMMON_PHAR_LEN           6

// PPP Session ID Register
#define W5500_COMMON_PSID               0x0024
#define W5500_COMMON_PSID_LEN           2

// PPP Maximum Receive Unit Register
#define W5500_COMMON_PMRU               0x0026
#define W5500_COMMON_PMRU_LEN           2
#define W5500_COMMON_PMRU_DEFAULT       0xFFFF

// Unreachable IP Address Register
#define W5500_COMMON_UIPR               0x0028
#define W5500_COMMON_UIPR_LEN           4

// Unreachable Port Register
#define W5500_COMMON_UPORTR             0x002C
#define W5500_COMMON_UPORTR_LEN         2

// PHY Configuration Register
#define W5500_COMMON_PHYCFGR            0x002E
#define W5500_COMMON_PHYCFGR_DEFAULT    0xB8

#define W5500_COMMON_PHYCFGR_RST                                ((uint8_t)(1u << 7))
#define W5500_COMMON_PHYCFGR_OPMD_OPMDC                         ((uint8_t)(1u << 6))
#define W5500_COMMON_PHYCFGR_OPMDC_10BT_H                       0
#define W5500_COMMON_PHYCFGR_OPMDC_10BT_F                       ((uint8_t)(1u << 3))
#define W5500_COMMON_PHYCFGR_OPMDC_100BT_H                      ((uint8_t)(1u << 4))
#define W5500_COMMON_PHYCFGR_OPMDC_100BT_F                      ((uint8_t)(1u << 4 | 1u << 3))
#define W5500_COMMON_PHYCFGR_OPMDC_100BT_H_AUTO_ENABLED         ((uint8_t)(1u << 5))
#define W5500_COMMON_PHYCFGR_POWER_DOWN_MODE                    ((uint8_t)(1u << 5 | 1u << 4))
#define W5500_COMMON_PHYCFGR_ALL_CAPABLE_AUTO_NEGOTIATION       (0b111u << 3)
#define W5500_COMMON_PHYCFGR_DPX_FULL                           ((uint8_t)(1u << 2))
#define W5500_COMMON_PHYCFGR_DPX_HALF                           0x00
#define W5500_COMMON_PHYCFGR_SPD_100MBPS                        ((uint8_t)(1u << 1))
#define W5500_COMMON_PHYCFGR_SPD_10MBPS                         0x00
#define W5500_COMMON_PHYCFGR_LNK_ON                             ((uint8_t)(1u << 0))
#define W5500_COMMON_PHYCFGR_LNK_OFF                            0x00

// Version Register
#define W5500_COMMON_VERSIONR           0x0039
#define W5500_COMMON_VERSIONR_VALUE     0x04

/* =============================================================================
 * SOCKET REGISTERS (Sn_xxx)
 * ============================================================================= */

// Socket n Mode Register
#define W5500_Sn_MR                     0x0000
#define W5500_Sn_MR_MULTI_MFEN          ((uint8_t)(1u << 7))
#define W5500_Sn_MR_BCASTB              ((uint8_t)(1u << 6))
#define W5500_Sn_MR_ND_MC_MMB           ((uint8_t)(1u << 5))
#define W5500_Sn_MR_UCASTB_MIP6B        ((uint8_t)(1u << 4))
#define W5500_Sn_MR_CLOSE               0x00
#define W5500_Sn_MR_TCP                 0x01
#define W5500_Sn_MR_UDP                 0x02
#define W5500_Sn_MR_MACRAW              0x04

// Socket n Command Register
#define W5500_Sn_CR                     0x0001
#define W5500_Sn_CR_OPEN                0x01
#define W5500_Sn_CR_LISTEN              0x02
#define W5500_Sn_CR_CONNECT             0x04
#define W5500_Sn_CR_DISCON              0x08
#define W5500_Sn_CR_CLOSE               0x10
#define W5500_Sn_CR_SEND                0x20
#define W5500_Sn_CR_SEND_MAC            0x21
#define W5500_Sn_CR_SEND_KEEP           0x22
#define W5500_Sn_CR_RECV                0x40

// Socket n Interrupt Register
#define W5500_Sn_IR                     0x0002
#define W5500_Sn_IR_SEND_OK             ((uint8_t)(1u << 4))
#define W5500_Sn_IR_TIMEOUT             ((uint8_t)(1u << 3))
#define W5500_Sn_IR_RECV                ((uint8_t)(1u << 2))
#define W5500_Sn_IR_DISCON              ((uint8_t)(1u << 1))
#define W5500_Sn_IR_CON                 ((uint8_t)(1u << 0))

// Socket n Status Register
#define W5500_Sn_SR                     0x0003
#define W5500_Sn_SR_CLOSED              0x00
#define W5500_Sn_SR_INIT                0x13
#define W5500_Sn_SR_LISTEN              0x14
#define W5500_Sn_SR_SYNSENT             0x15
#define W5500_Sn_SR_SYNRECV             0x16
#define W5500_Sn_SR_ESTABLISHED         0x17
#define W5500_Sn_SR_FIN_WAIT            0x18
#define W5500_Sn_SR_CLOSING             0x1A
#define W5500_Sn_SR_TIME_WAIT           0x1B
#define W5500_Sn_SR_CLOSE_WAIT          0x1C
#define W5500_Sn_SR_LAST_ACK            0x1D
#define W5500_Sn_SR_UDP                 0x22
#define W5500_Sn_SR_MACRAW              0x42

// Socket n Source Port Register
#define W5500_Sn_PORT                   0x0004
#define W5500_Sn_PORT_LEN               2

// Socket n Destination Hardware Address Register
#define W5500_Sn_DHAR                   0x0006
#define W5500_Sn_DHAR_LEN               6

// Socket n Destination IP Address Register
#define W5500_Sn_DIPR                   0x000C
#define W5500_Sn_DIPR_LEN               4

// Socket n Destination Port Register
#define W5500_Sn_DPORT                  0x0010
#define W5500_Sn_DPORT_LEN              2

// Socket n Maximum Segment Size Register
#define W5500_Sn_MSSR                   0x0012
#define W5500_Sn_MSSR_LEN               2

// Socket n IP Type of Service Register
#define W5500_Sn_TOS                    0x0015

// Socket n Time to Live Register
#define W5500_Sn_TTL                    0x0016

// Socket n RX Buffer Size Register
#define W5500_Sn_RXBUF_SIZE             0x001E
#define W5500_Sn_RXBUF_SIZE_DEFAULT     2

// Socket n TX Buffer Size Register
#define W5500_Sn_TXBUF_SIZE             0x001F
#define W5500_Sn_TXBUF_SIZE_DEFAULT     2

// Socket n TX Free Size Register
#define W5500_Sn_TX_FSR                 0x0020
#define W5500_Sn_TX_FSR_LEN             2

// Socket n TX Read Pointer Register
#define W5500_Sn_TX_RD                  0x0022
#define W5500_Sn_TX_RD_LEN              2

// Socket n TX Write Pointer Register
#define W5500_Sn_TX_WR                  0x0024
#define W5500_Sn_TX_WR_LEN              2

// Socket n Received Size Register
#define W5500_Sn_RX_RSR                 0x0026
#define W5500_Sn_RX_RSR_LEN             2

// Socket n RX Read Pointer Register
#define W5500_Sn_RX_RD                  0x0028
#define W5500_Sn_RX_RD_LEN              2

// Socket n RX Write Pointer Register
#define W5500_Sn_RX_WR                  0x002A
#define W5500_Sn_RX_WR_LEN              2

// Socket n Interrupt Mask Register
#define W5500_Sn_IMR                    0x002C
#define W5500_Sn_IMR_SENDOK             ((uint8_t)(1u << 4))
#define W5500_Sn_IMR_TIMEOUT            ((uint8_t)(1u << 3))
#define W5500_Sn_IMR_RECV               ((uint8_t)(1u << 2))
#define W5500_Sn_IMR_DISCON             ((uint8_t)(1u << 1))
#define W5500_Sn_IMR_CON                ((uint8_t)(1u << 0))

// Socket n Fragment Offset Register
#define W5500_Sn_FRAG                   0x002D
#define W5500_Sn_FRAG_LEN               2
#define W5500_Sn_FRAG_DEFAULT           0x4000

// Socket n Keep Alive Timer Register
#define W5500_Sn_KPALVTR                0x002F

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_DRIVER_W5500_W5500_REGS_H_ */
