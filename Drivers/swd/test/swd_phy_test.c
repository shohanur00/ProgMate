#include <stdint.h>
#include <stdbool.h>

#include "logger/frontend/logger.h"
#include "swd/physical_layer/swd_phy.h"
#include "swd/test/swd_phy_test.h"


/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

/*
 * Half clock period delay (busy-loop iterations).
 * Keep it slow during bring-up (~100-500 kHz), then reduce.
 * The value depends on the core clock; measure with a scope or logic
 * analyzer and tune accordingly.
 */
#ifndef SWD_TEST_HALF_PERIOD_LOOPS
#define SWD_TEST_HALF_PERIOD_LOOPS   50U
#endif

/* Number of idle (SWDIO low) clocks after a line reset. */
#define SWD_TEST_IDLE_CYCLES_AFTER_RESET   8U

/* Number of idle (SWDIO low) clocks after a transfer. */
#define SWD_TEST_IDLE_CYCLES_AFTER_XFER    8U

/*
 * DP IDCODE (DPIDR) read request:
 * Start=1 APnDP=0 RnW=1 A[3:2]=00 Parity=1 Stop=0 Park=1
 */
#define SWD_TEST_REQ_READ_DPIDR   0xA5U

/* JTAG-to-SWD selection sequence (LSB first). */
#define SWD_TEST_JTAG_TO_SWD      0xE79EU

#define SWD_TEST_ACK_OK           0x01U


/* --------------------------------------------------------------------------
 * Private SWD test helpers
 * -------------------------------------------------------------------------- */

static void SWD_PHY_TestDelay(void)
{
    for (volatile uint32_t i = 0U; i < SWD_TEST_HALF_PERIOD_LOOPS; i++)
    {
        __asm volatile ("nop");
    }
}


/*
 * Host -> target bit.
 * Data is placed while the clock is low; the target samples it on the
 * rising edge. The clock is left low afterwards.
 */
static void SWD_PHY_TestWriteBit(bool bit)
{
    SWD_PHY_WriteClock(false);
    SWD_PHY_WriteData(bit);
    SWD_PHY_TestDelay();

    SWD_PHY_WriteClock(true);
    SWD_PHY_TestDelay();

    SWD_PHY_WriteClock(false);
}


/*
 * Target -> host bit.
 * The target changes SWDIO after the rising edge, so the host samples at
 * the end of the clock-low phase (just before the next rising edge).
 */
static bool SWD_PHY_TestReadBit(void)
{
    bool bit;

    SWD_PHY_WriteClock(false);
    SWD_PHY_TestDelay();

    bit = SWD_PHY_ReadData();

    SWD_PHY_WriteClock(true);
    SWD_PHY_TestDelay();

    SWD_PHY_WriteClock(false);

    return bit;
}


static void SWD_PHY_TestWriteSequence(uint16_t sequence,
                                      uint8_t bit_count)
{
    for (uint8_t i = 0U; i < bit_count; i++)
    {
        SWD_PHY_TestWriteBit(((sequence >> i) & 1U) != 0U);
    }
}


/* Clock out idle cycles with SWDIO low. The host must be driving SWDIO. */
static void SWD_PHY_TestIdle(uint8_t cycles)
{
    for (uint8_t i = 0U; i < cycles; i++)
    {
        SWD_PHY_TestWriteBit(false);
    }
}


/*
 * Host takes the line back after a target-driven phase: one turnaround
 * clock while SWDIO is still an input, then switch to output (low).
 */
static void SWD_PHY_TestTurnaroundToHost(void)
{
    (void)SWD_PHY_TestReadBit();
    SWD_PHY_SetDataOutput();
    SWD_PHY_WriteData(false);
}


static uint8_t SWD_PHY_TestParity32(uint32_t value)
{
    uint8_t parity = 0U;

    for (uint8_t i = 0U; i < 32U; i++)
    {
        parity ^= (uint8_t)((value >> i) & 1U);
    }

    return parity;
}


/* --------------------------------------------------------------------------
 * Public test function
 * -------------------------------------------------------------------------- */

void SWD_PHY_TestReadDPIDCODE(void)
{
    uint8_t  ack    = 0U;
    uint8_t  parity = 0U;
    uint32_t idcode = 0U;

    LOG_INFO(LOG_MODULE_SYSTEM,
             "SWD: Starting DP IDCODE read");

    /* SWDIO as output, SWCLK starts low. */
    SWD_PHY_SetDataOutput();
    SWD_PHY_WriteClock(false);

    /* 1. Line reset: SWDIO high for at least 50 clocks (done in SWD_PHY_LineReset). */
    SWD_PHY_LineReset();

    /* 2. JTAG-to-SWD selection sequence. */
    SWD_PHY_TestWriteSequence(SWD_TEST_JTAG_TO_SWD, 16U);

    /* 3. Second line reset. */
    SWD_PHY_LineReset();

    /* 4. Idle cycles (SWDIO low) so the next start bit is clearly distinguishable. */
    SWD_PHY_TestIdle(SWD_TEST_IDLE_CYCLES_AFTER_RESET);

    /* 5. DPIDR read request. */
    SWD_PHY_TestWriteSequence(SWD_TEST_REQ_READ_DPIDR, 8U);

    /* 6. Host releases SWDIO; one turnaround clock. */
    SWD_PHY_SetDataInput();
    (void)SWD_PHY_TestReadBit();

    /* 7. 3-bit ACK, LSB first. */
    for (uint8_t i = 0U; i < 3U; i++)
    {
        ack |= (uint8_t)((uint8_t)SWD_PHY_TestReadBit() << i);
    }

    if (ack != SWD_TEST_ACK_OK)
    {
        /* Take the line back, idle, then line reset to resync for the next attempt. */
        SWD_PHY_TestTurnaroundToHost();
        SWD_PHY_TestIdle(SWD_TEST_IDLE_CYCLES_AFTER_XFER);
        SWD_PHY_LineReset();
        SWD_PHY_TestIdle(SWD_TEST_IDLE_CYCLES_AFTER_RESET);

        switch (ack)
        {
            case 0x07U:
                LOG_ERROR(LOG_MODULE_SYSTEM,
                          "SWD: ACK=0x7, no target response (check wiring, GND, power, NRST)");
                break;

            case 0x00U:
                LOG_ERROR(LOG_MODULE_SYSTEM,
                          "SWD: ACK=0x0, line stuck low or target not responding");
                break;

            case 0x02U:
                LOG_ERROR(LOG_MODULE_SYSTEM,
                          "SWD: ACK=WAIT");
                break;

            case 0x04U:
                LOG_ERROR(LOG_MODULE_SYSTEM,
                          "SWD: ACK=FAULT");
                break;

            default:
                LOG_ERROR(LOG_MODULE_SYSTEM,
                          "SWD: Invalid ACK: 0x%X", ack);
                break;
        }

        return;
    }

    /* 8. 32-bit IDCODE, LSB first. */
    for (uint8_t i = 0U; i < 32U; i++)
    {
        idcode |= ((uint32_t)SWD_PHY_TestReadBit() << i);
    }

    /* 9. Data parity bit. */
    parity = (uint8_t)SWD_PHY_TestReadBit();

    /* 10. Turnaround: host takes the line back, then idle. */
    SWD_PHY_TestTurnaroundToHost();
    SWD_PHY_TestIdle(SWD_TEST_IDLE_CYCLES_AFTER_XFER);

    /* 11. Parity check. */
    if (SWD_PHY_TestParity32(idcode) != parity)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "SWD: IDCODE parity error (IDCODE=0x%X)", idcode);
        return;
    }

    LOG_INFO(LOG_MODULE_SYSTEM,
             "SWD: DP IDCODE = 0x%X", idcode);
}