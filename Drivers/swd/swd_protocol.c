
/**
 * @file    swd_protocol.c
 * @brief   SWD protocol-layer implementation.
 *
 * @target  STM32G431CBT6 -> STM32G030F6P6
 *
 * @details Implements SWD request generation, ACK decoding,
 *          data transfer, turnaround cycles and parity checking.
 *
 * @note    Blocking implementation for initial SWD bring-up.
 */

#include "swd_protocol.h"
#include "swd_phy.h"
#include "logger/frontend/logger.h"

#define SWD_REQUEST_START          (1U)
#define SWD_REQUEST_STOP           (0U)
#define SWD_REQUEST_PARK           (1U)

#define SWD_DP_REG_IDCODE          (0x00U)
#define SWD_DP_REG_ABORT           (0x00U)
#define SWD_DP_REG_CTRL_STAT       (0x04U)
#define SWD_DP_REG_SELECT          (0x08U)
#define SWD_DP_REG_RDBUFF           (0x0CU)

#define SWD_IDLE_CYCLES            (1U)

/* -------------------------------------------------------------------------- */
/* Private functions                                                         */
/* -------------------------------------------------------------------------- */

static void SWD_ClockCycle(void)
{
    SWD_PHY_WriteClock(true);
    SWD_PHY_WriteClock(false);
}

static void SWD_WriteBit(bool bit)
{
    SWD_PHY_WriteData(bit);
    SWD_ClockCycle();
}

static bool SWD_ReadBit(void)
{
    bool bit;

    SWD_PHY_WriteClock(true);
    bit = SWD_PHY_ReadData();
    SWD_PHY_WriteClock(false);

    return bit;
}

static void SWD_WriteBits(uint32_t data, uint8_t bit_count)
{
    for (uint8_t i = 0U; i < bit_count; i++)
    {
        SWD_WriteBit(((data >> i) & 1U) != 0U);
    }
}

static uint32_t SWD_ReadBits(uint8_t bit_count)
{
    uint32_t data = 0U;

    for (uint8_t i = 0U; i < bit_count; i++)
    {
        if (SWD_ReadBit())
        {
            data |= (1UL << i);
        }
    }

    return data;
}

static uint8_t SWD_CalculateParity32(uint32_t data)
{
    uint8_t parity = 0U;

    for (uint8_t i = 0U; i < 32U; i++)
    {
        parity ^= (uint8_t)((data >> i) & 1U);
    }

    return parity;
}

static uint8_t SWD_BuildRequest(bool ap_access,
                                bool read,
                                uint8_t address)
{
    uint8_t apndp = ap_access ? 1U : 0U;
    uint8_t rnw   = read ? 1U : 0U;
    uint8_t a2    = (uint8_t)((address >> 2U) & 1U);
    uint8_t a3    = (uint8_t)((address >> 3U) & 1U);

    uint8_t parity = apndp ^ rnw ^ a2 ^ a3;

    return (uint8_t)(
          (SWD_REQUEST_START << 0U)
        | (apndp             << 1U)
        | (rnw               << 2U)
        | (a2                << 3U)
        | (a3                << 4U)
        | (parity             << 5U)
        | (SWD_REQUEST_STOP  << 6U)
        | (SWD_REQUEST_PARK  << 7U));
}

static SWD_ACK_t SWD_ReadAck(void)
{
    uint8_t ack = (uint8_t)SWD_ReadBits(3U);

    LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Raw ACK = 0x%X", (unsigned int)ack);

    switch (ack)
    {
        case 0x01U:
            LOG_INFO(LOG_MODULE_SYSTEM,"SWD: ACK OK");
            return SWD_ACK_OK;

        case 0x02U:
            LOG_INFO(LOG_MODULE_SYSTEM,"SWD: ACK WAIT");
            return SWD_ACK_WAIT;

        case 0x04U:
            LOG_INFO(LOG_MODULE_SYSTEM,"SWD: ACK FAULT");
            return SWD_ACK_FAULT;

        default:
            LOG_ERROR(LOG_MODULE_SYSTEM,"SWD: Invalid ACK");
            return SWD_ACK_INVALID;
    }
}

static void SWD_SendIdleCycles(uint8_t cycles)
{
    SWD_PHY_SetDataOutput();
    SWD_PHY_WriteData(true);

    for (uint8_t i = 0U; i < cycles; i++)
    {
        SWD_ClockCycle();
    }
}

/*
 * Performs one raw DP/AP register transfer.
 *
 * AP reads are posted reads: the returned data is not necessarily
 * the value requested. SWD_Protocol_AP_Read() handles this using RDBUFF.
 */
static SWD_Status_t SWD_Transfer(bool ap_access,
                                 bool read,
                                 uint8_t address,
                                 uint32_t *read_data,
                                 uint32_t write_data)
{
    SWD_ACK_t ack;
    uint8_t request;

    if ((read && (read_data == (uint32_t *)0)))
    {
        return SWD_STATUS_INVALID_ARGUMENT;
    }

    request = SWD_BuildRequest(ap_access, read, address);

    /* Host drives the 8-bit request. */
    SWD_PHY_SetDataOutput();
    SWD_PHY_WriteClock(false);
    SWD_WriteBits(request, 8U);

    /* Target takes control of SWDIO for turnaround and ACK. */
    SWD_PHY_SetDataInput();
    SWD_ClockCycle();

    ack = SWD_ReadAck();

    if (ack != SWD_ACK_OK)
    {
        /*
         * Recover bus ownership after ACK.
         * The target may have returned WAIT, FAULT or an invalid ACK.
         */
        SWD_ClockCycle();
        SWD_SendIdleCycles(SWD_IDLE_CYCLES);

        if (ack == SWD_ACK_WAIT)
        {
            return SWD_STATUS_WAIT;
        }

        if (ack == SWD_ACK_FAULT)
        {
            return SWD_STATUS_FAULT;
        }

        return SWD_STATUS_ERROR;
    }

    if (read)
    {
        uint32_t data;
        uint8_t received_parity;

        data = SWD_ReadBits(32U);
        received_parity = (uint8_t)SWD_ReadBits(1U);

        /* Target-to-host turnaround. */
        SWD_ClockCycle();
        SWD_PHY_SetDataOutput();
        SWD_PHY_WriteData(true);

        *read_data = data;

        if (SWD_CalculateParity32(data) != received_parity)
        {
            SWD_SendIdleCycles(SWD_IDLE_CYCLES);
            return SWD_STATUS_PARITY_ERROR;
        }
    }
    else
    {
        /* Target-to-host turnaround before host sends write data. */
        SWD_ClockCycle();
        SWD_PHY_SetDataOutput();

        SWD_WriteBits(write_data, 32U);
        SWD_WriteBit(SWD_CalculateParity32(write_data) != 0U);
    }

    SWD_SendIdleCycles(SWD_IDLE_CYCLES);

    return SWD_STATUS_OK;
}

/* -------------------------------------------------------------------------- */
/* Public functions                                                          */
/* -------------------------------------------------------------------------- */

SWD_Status_t SWD_Protocol_Init(void)
{
    if (!SWD_PHY_Init())
    {
        return SWD_STATUS_ERROR;
    }

    return SWD_Protocol_LineReset();
}

SWD_Status_t SWD_Protocol_LineReset(void)
{
    SWD_PHY_LineReset();

    /*
     * Leave the bus idle with SWDIO high.
     * Target DP IDCODE can now be requested.
     */
    SWD_PHY_WriteClock(false);
    SWD_PHY_SetDataOutput();
    SWD_PHY_WriteData(true);

    return SWD_STATUS_OK;
}

SWD_Status_t SWD_Protocol_ReadIDCODE(uint32_t *idcode)
{
    if (idcode == (uint32_t *)0)
    {
        return SWD_STATUS_INVALID_ARGUMENT;
    }

    return SWD_Protocol_DP_Read(SWD_DP_REG_IDCODE, idcode);
}

SWD_Status_t SWD_Protocol_DP_Read(uint8_t address, uint32_t *data)
{
    return SWD_Transfer(false, true, address, data, 0U);
}

SWD_Status_t SWD_Protocol_DP_Write(uint8_t address, uint32_t data)
{
    return SWD_Transfer(false, false, address, (uint32_t *)0, data);
}

SWD_Status_t SWD_Protocol_AP_Read(uint8_t ap_number,
                                  uint8_t address,
                                  uint32_t *data)
{
    uint32_t select;
    uint32_t discarded_data;
    SWD_Status_t status;

    if (data == (uint32_t *)0)
    {
        return SWD_STATUS_INVALID_ARGUMENT;
    }

    /*
     * Select the AP and its register bank:
     * SELECT[31:24] = APSEL
     * SELECT[7:4]   = APBANKSEL
     */
    select = ((uint32_t)ap_number << 24U)
           | ((uint32_t)(address & 0xF0U) >> 4U << 4U);

    status = SWD_Protocol_DP_Write(SWD_DP_REG_SELECT, select);

    if (status != SWD_STATUS_OK)
    {
        return status;
    }

    /* First AP read initiates the posted read. */
    status = SWD_Transfer(true, true, address,
                          &discarded_data, 0U);

    if (status != SWD_STATUS_OK)
    {
        return status;
    }

    /* Retrieve the result of the posted AP read. */
    return SWD_Protocol_DP_Read(SWD_DP_REG_RDBUFF, data);
}

SWD_Status_t SWD_Protocol_AP_Write(uint8_t ap_number,
                                   uint8_t address,
                                   uint32_t data)
{
    uint32_t select;
    SWD_Status_t status;

    select = ((uint32_t)ap_number << 24U)
           | ((uint32_t)(address & 0xF0U) >> 4U << 4U);

    status = SWD_Protocol_DP_Write(SWD_DP_REG_SELECT, select);

    if (status != SWD_STATUS_OK)
    {
        return status;
    }

    return SWD_Transfer(true, false, address, (uint32_t *)0, data);
}