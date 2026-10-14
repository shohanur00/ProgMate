#ifndef SWD_PROTOCOL_H
#define SWD_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#include "../swd_types.h"

/*
 * SWD protocol layer.
 *
 * Pure data: builds the packet to send, decodes the packet received, and
 * defines every protocol constant and fixed sequence. It never touches
 * hardware and never calls swd_phy. swd.c carries the packets to the PHY.
 */

/* --------------------------------------------------------------------------
 * Protocol constants
 * -------------------------------------------------------------------------- */

#define SWD_PROTOCOL_REQUEST_BITS           8U
#define SWD_PROTOCOL_ACK_BITS               3U
#define SWD_PROTOCOL_DATA_BITS              32U
#define SWD_PROTOCOL_PARITY_BITS            1U

/* Idle clocks (SWDIO low) after every transfer. */
#define SWD_PROTOCOL_IDLE_AFTER_TRANSFER    8U

/* DP ABORT register: writing this clears the sticky error flags. */
#define SWD_PROTOCOL_DP_ABORT_ADDR          0x00U
#define SWD_PROTOCOL_ABORT_CLEAR_STICKY     0x0000001EUL


/* --------------------------------------------------------------------------
 * Types
 * -------------------------------------------------------------------------- */

/* Request fields. Start, Stop and Park are fixed, so they are not stored. */
typedef struct
{
    bool    ap;         /* APnDP: false = DP, true = AP                     */
    bool    read;       /* RnW:   true = read, false = write                */
    uint8_t addr;       /* register address, bits [3:2] go on the wire      */
    uint8_t parity;     /* parity over APnDP, RnW, A2, A3                   */
} SWD_Request_t;

/* Packet sent by the host. */
typedef struct
{
    SWD_Request_t request;
    uint32_t      data;          /* data word (write only)                  */
    uint8_t       data_parity;   /* data parity bit (write only)            */
} SWD_TxPacket_t;

/* Packet received from the target (raw, not yet decoded). */
typedef struct
{
    uint8_t  ack;           /* raw 3-bit ACK                                */
    uint32_t data;          /* data word (read, ACK OK only)                */
    uint8_t  data_parity;   /* data parity bit (read, ACK OK only)          */
} SWD_RxPacket_t;

/* One raw bit field of a fixed sequence, sent LSB first. */
typedef struct
{
    uint32_t value;
    uint8_t  bits;          /* 1..32 */
} SWD_Sequence_t;


/* --------------------------------------------------------------------------
 * Packet building
 * -------------------------------------------------------------------------- */

SWD_Status_t SWD_Protocol_BuildPacket(bool ap, bool read, uint8_t addr,
                                      uint32_t data, SWD_TxPacket_t *tx);

/* Serialize a request into the 8-bit word that is sent LSB first. */
uint8_t SWD_Protocol_PackRequest(const SWD_Request_t *req);


/* --------------------------------------------------------------------------
 * Packet decoding
 * -------------------------------------------------------------------------- */

/* OK / WAIT / FAULT / NO_ACK from the raw 3-bit ACK. */
SWD_Status_t SWD_Protocol_DecodeAck(uint8_t ack);

/* True when this ACK is followed by a data phase (ACK OK only). */
bool SWD_Protocol_AckHasDataPhase(uint8_t ack);

/* ACK check plus, for reads, parity check. Copies the data on success. */
SWD_Status_t SWD_Protocol_DecodePacket(bool read, const SWD_RxPacket_t *rx,
                                       uint32_t *data);


/* --------------------------------------------------------------------------
 * Fixed sequences
 * -------------------------------------------------------------------------- */

/*
 * Connect sequence: line reset, JTAG-to-SWD switch, line reset, idle.
 * Returns the table and stores the entry count in *count.
 */
const SWD_Sequence_t *SWD_Protocol_GetConnectSequence(uint8_t *count);

#endif /* SWD_PROTOCOL_H */