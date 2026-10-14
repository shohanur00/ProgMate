#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "swd_protocol.h"


/* --------------------------------------------------------------------------
 * Protocol definitions
 * -------------------------------------------------------------------------- */

/* ACK values (3 bits, LSB first). */
#define SWD_ACK_OK                  0x01U
#define SWD_ACK_WAIT                0x02U
#define SWD_ACK_FAULT               0x04U

/* Request byte bit positions. */
#define SWD_REQ_START               0x01U
#define SWD_REQ_APNDP_POS           1U
#define SWD_REQ_RNW_POS             2U
#define SWD_REQ_A2_POS              3U
#define SWD_REQ_A3_POS              4U
#define SWD_REQ_PARITY_POS          5U
#define SWD_REQ_STOP                0x00U       /* bit 6: Stop = 0 */
#define SWD_REQ_PARK                0x80U

/* Register address bits that go on the wire (A[3:2]). */
#define SWD_ADDR_WIRE_MASK          0x0CU

/* Connect sequence building blocks. */
#define SWD_LINE_RESET_WORD         0xFFFFFFFFUL    /* 32 clocks, SWDIO high */
#define SWD_JTAG_TO_SWD_SEQUENCE    0xE79EUL
#define SWD_JTAG_TO_SWD_BITS        16U
#define SWD_IDLE_AFTER_RESET_BITS   8U

/*
 * line reset (2 x 32 = 64 clocks, >= 50 required)
 * JTAG-to-SWD switch
 * line reset
 * idle (SWDIO low)
 */
static const SWD_Sequence_t s_connect_sequence[] =
{
    { SWD_LINE_RESET_WORD,      32U                         },
    { SWD_LINE_RESET_WORD,      32U                         },
    { SWD_JTAG_TO_SWD_SEQUENCE, SWD_JTAG_TO_SWD_BITS        },
    { SWD_LINE_RESET_WORD,      32U                         },
    { SWD_LINE_RESET_WORD,      32U                         },
    { 0x00000000UL,             SWD_IDLE_AFTER_RESET_BITS   }
};


/* --------------------------------------------------------------------------
 * Private helpers
 * -------------------------------------------------------------------------- */

static uint8_t SWD_Protocol_Parity32(uint32_t value)
{
    uint8_t parity = 0U;

    for (uint8_t i = 0U; i < 32U; i++)
    {
        parity ^= (uint8_t)((value >> i) & 1U);
    }

    return parity;
}


/* Request parity covers APnDP, RnW, A2 and A3 only. */
static uint8_t SWD_Protocol_RequestParity(bool ap, bool read, uint8_t addr)
{
    uint8_t a2 = (uint8_t)((addr >> 2) & 1U);
    uint8_t a3 = (uint8_t)((addr >> 3) & 1U);

    return (uint8_t)(((ap ? 1U : 0U) ^ (read ? 1U : 0U) ^ a2 ^ a3) & 1U);
}


/* --------------------------------------------------------------------------
 * Packet building
 * -------------------------------------------------------------------------- */

/*
 * Serialize the request struct. Bit order is set explicitly with shifts
 * (no bitfields), so it does not depend on the compiler.
 *
 *   Start(1) APnDP RnW A2 A3 Parity Stop(0) Park(1), LSB first.
 */
uint8_t SWD_Protocol_PackRequest(const SWD_Request_t *req)
{
    uint8_t a2 = (uint8_t)((req->addr >> 2) & 1U);
    uint8_t a3 = (uint8_t)((req->addr >> 3) & 1U);

    return (uint8_t)(SWD_REQ_START |
                     ((req->ap   ? 1U : 0U) << SWD_REQ_APNDP_POS)   |
                     ((req->read ? 1U : 0U) << SWD_REQ_RNW_POS)     |
                     (a2                    << SWD_REQ_A2_POS)      |
                     (a3                    << SWD_REQ_A3_POS)      |
                     ((req->parity & 1U)    << SWD_REQ_PARITY_POS)  |
                     SWD_REQ_STOP |
                     SWD_REQ_PARK);
}


SWD_Status_t SWD_Protocol_BuildPacket(bool ap, bool read, uint8_t addr,
                                      uint32_t data, SWD_TxPacket_t *tx)
{
    if (tx == NULL)
    {
        return SWD_STATUS_PARAM;
    }

    tx->request.ap     = ap;
    tx->request.read   = read;
    tx->request.addr   = (uint8_t)(addr & SWD_ADDR_WIRE_MASK);
    tx->request.parity = SWD_Protocol_RequestParity(ap, read, addr);

    tx->data        = read ? 0U : data;
    tx->data_parity = read ? 0U : SWD_Protocol_Parity32(data);

    return SWD_STATUS_OK;
}


/* --------------------------------------------------------------------------
 * Packet decoding
 * -------------------------------------------------------------------------- */

SWD_Status_t SWD_Protocol_DecodeAck(uint8_t ack)
{
    switch (ack)
    {
        case SWD_ACK_OK:    return SWD_STATUS_OK;
        case SWD_ACK_WAIT:  return SWD_STATUS_WAIT;
        case SWD_ACK_FAULT: return SWD_STATUS_FAULT;
        default:            return SWD_STATUS_NO_ACK;   /* 0x7, 0x0, others */
    }
}


bool SWD_Protocol_AckHasDataPhase(uint8_t ack)
{
    return (ack == SWD_ACK_OK);
}


SWD_Status_t SWD_Protocol_DecodePacket(bool read, const SWD_RxPacket_t *rx,
                                       uint32_t *data)
{
    SWD_Status_t status;

    if (rx == NULL)
    {
        return SWD_STATUS_PARAM;
    }

    status = SWD_Protocol_DecodeAck(rx->ack);
    if (status != SWD_STATUS_OK)
    {
        return status;
    }

    if (read)
    {
        if (SWD_Protocol_Parity32(rx->data) != rx->data_parity)
        {
            return SWD_STATUS_PARITY;
        }

        if (data != NULL)
        {
            *data = rx->data;
        }
    }

    return SWD_STATUS_OK;
}


/* --------------------------------------------------------------------------
 * Fixed sequences
 * -------------------------------------------------------------------------- */

const SWD_Sequence_t *SWD_Protocol_GetConnectSequence(uint8_t *count)
{
    if (count != NULL)
    {
        *count = (uint8_t)(sizeof(s_connect_sequence) / sizeof(s_connect_sequence[0]));
    }

    return s_connect_sequence;
}