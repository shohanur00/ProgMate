#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "swd.h"
#include "physical_layer/swd_phy.h"
#include "protocol/swd_protocol.h"



/* --------------------------------------------------------------------------
 * Policy
 * -------------------------------------------------------------------------- */

#define SWD_WAIT_RETRIES    32U


/* --------------------------------------------------------------------------
 * Private
 * -------------------------------------------------------------------------- */

/*
 * One transaction, no retry.
 *
 * Read :  REQ -> TRN -> ACK -> DATA -> PARITY -> TRN -> idle
 * Write:  REQ -> TRN -> ACK -> TRN -> DATA -> PARITY -> idle
 * Not OK: REQ -> TRN -> ACK -> TRN -> idle   (no data phase)
 */
static SWD_Status_t SWD_TransferOnce(bool ap, bool read,
                                     uint8_t addr, uint32_t *data)
{
    SWD_TxPacket_t tx;
    SWD_RxPacket_t rx;
    uint32_t write_value = ((!read) && (data != NULL)) ? *data : 0U;

    rx.ack         = 0U;
    rx.data        = 0U;
    rx.data_parity = 0U;

    /* Protocol: build the packet. */
    (void)SWD_Protocol_BuildPacket(ap, read, addr, write_value, &tx);

    /* PHY: send the request, then the target takes the line. */
    SWD_PHY_WriteBits(SWD_Protocol_PackRequest(&tx.request),
                      SWD_PROTOCOL_REQUEST_BITS);
    SWD_PHY_TurnaroundToTarget();

    rx.ack = (uint8_t)SWD_PHY_ReadBits(SWD_PROTOCOL_ACK_BITS);

    if (!SWD_Protocol_AckHasDataPhase(rx.ack))
    {
        SWD_PHY_TurnaroundToHost();
    }
    else if (tx.request.read)
    {
        rx.data        = SWD_PHY_ReadBits(SWD_PROTOCOL_DATA_BITS);
        rx.data_parity = (uint8_t)SWD_PHY_ReadBits(SWD_PROTOCOL_PARITY_BITS);
        SWD_PHY_TurnaroundToHost();
    }
    else
    {
        SWD_PHY_TurnaroundToHost();
        SWD_PHY_WriteBits(tx.data, SWD_PROTOCOL_DATA_BITS);
        SWD_PHY_WriteBits(tx.data_parity, SWD_PROTOCOL_PARITY_BITS);
    }

    SWD_PHY_WriteBits(0U, SWD_PROTOCOL_IDLE_AFTER_TRANSFER);

    /* Protocol: decode what came back. */
    return SWD_Protocol_DecodePacket(tx.request.read, &rx, data);
}


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

SWD_Status_t SWD_Init(void)
{
    return SWD_PHY_Init() ? SWD_STATUS_OK : SWD_STATUS_PARAM;
}


SWD_Status_t SWD_Connect(void)
{
    uint8_t count = 0U;
    const SWD_Sequence_t *sequence = SWD_Protocol_GetConnectSequence(&count);

    for (uint8_t i = 0U; i < count; i++)
    {
        SWD_PHY_WriteBits(sequence[i].value, sequence[i].bits);
    }

    return SWD_STATUS_OK;
}


SWD_Status_t SWD_Transfer(bool ap, bool read, uint8_t addr, uint32_t *data)
{
    SWD_Status_t status;
    uint8_t retries = SWD_WAIT_RETRIES;

    if (data == NULL)
    {
        return SWD_STATUS_PARAM;
    }

    for (;;)
    {
        status = SWD_TransferOnce(ap, read, addr, data);

        if (status != SWD_STATUS_WAIT)
        {
            break;
        }

        if (retries == 0U)
        {
            return SWD_STATUS_WAIT;
        }

        retries--;
    }

    if (status == SWD_STATUS_FAULT)
    {
        /* Clear the sticky error flags, otherwise every transfer keeps faulting. */
        uint32_t abort_value = SWD_PROTOCOL_ABORT_CLEAR_STICKY;

        (void)SWD_TransferOnce(false, false, SWD_PROTOCOL_DP_ABORT_ADDR, &abort_value);
    }

    return status;
}