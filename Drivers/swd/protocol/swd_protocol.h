#ifndef SWD_PROTOCOL_H
#define SWD_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#include "swd/common/swd_types.h"

/*
 * SWD protocol layer.
 *
 * Builds the packet to send, decodes the packet received, and sequences
 * the transaction phases. It uses swd_phy only to move raw bit fields.
 */

/* Packet sent by the host. */
typedef struct
{
    uint8_t  request;       /* 8-bit request word (LSB first on the wire)   */
    uint32_t data;          /* data word (write only)                       */
    uint8_t  data_parity;   /* data parity bit (write only)                 */
} SWD_TxPacket_t;

/* Packet received from the target (raw, not yet decoded). */
typedef struct
{
    uint8_t  ack;           /* raw 3-bit ACK                                */
    uint32_t data;          /* data word (read, ACK OK only)                */
    uint8_t  data_parity;   /* data parity bit (read, ACK OK only)          */
} SWD_RxPacket_t;

/* Packet building / decoding. No hardware access, testable on a PC. */
SWD_Status_t SWD_Protocol_BuildPacket(bool ap, bool read, uint8_t addr,
                                      uint32_t data, SWD_TxPacket_t *tx);

SWD_Status_t SWD_Protocol_DecodePacket(bool read, const SWD_RxPacket_t *rx,
                                       uint32_t *data);

/* Line reset, JTAG-to-SWD switch, line reset, idle cycles. */
SWD_Status_t SWD_Protocol_Connect(void);

/*
 * One full transaction: build, send, receive, decode.
 * Retries on WAIT, clears sticky errors on FAULT.
 *
 *   ap   : false = DP register, true = AP register
 *   read : true = read, false = write
 *   addr : register address, only bits [3:2] go on the wire
 *   data : read -> receives the value, write -> holds the value to send
 */
SWD_Status_t SWD_Protocol_Transfer(bool ap, bool read, uint8_t addr, uint32_t *data);

#endif /* SWD_PROTOCOL_H */