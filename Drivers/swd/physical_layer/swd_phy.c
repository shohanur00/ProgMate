
/**
 * @file    swd_phy.c
 * @brief   SWD physical-layer implementation.
 *
 * @target  STM32G431CBT6
 *
 * @details Controls SWCLK and SWDIO through the GPIO driver.
 */

#include "swd_phy.h"

#include "gpio/gpio.h"
#include "cdefs/cdefs.h"

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

#define SWD_PHY_LINE_RESET_CYCLES    (56U)
#ifndef SWD_TEST_HALF_PERIOD_LOOPS
#define SWD_TEST_HALF_PERIOD_LOOPS    50U
#endif

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



static void SWD_PHY_WriteSWCLK(bool state)
{
    GPIO_Write(
        SWD_SWCLK,
        (state == true) ? GPIO_HIGH : GPIO_LOW
    );
}


static void SWD_PHY_WriteSWDIO(bool state)
{
    GPIO_Write(
        SWD_SWDIO,
        (state == true) ? GPIO_HIGH : GPIO_LOW
    );
}


static bool SWD_PHY_ReadSWDIO(void)
{
    return (GPIO_Read(SWD_SWDIO) == GPIO_HIGH);
}


static void SWD_PHY_SetSWDIOOutput(void)
{
    GPIO_Mode_t mode = GPIO_MODE_OUTPUT;

    /*
     * Preload the output latch HIGH before enabling output mode.
     * This avoids briefly driving LOW when the output driver
     * becomes active.
     */
    GPIO_Write(SWD_SWDIO, GPIO_HIGH);

    GPIO_Ioctl(
        SWD_SWDIO,
        GPIO_IOCTL_SET_MODE,
        &mode
    );
}


static void SWD_PHY_SetSWDIOInput(void)
{
    GPIO_Mode_t mode = GPIO_MODE_INPUT;

    GPIO_Ioctl(
        SWD_SWDIO,
        GPIO_IOCTL_SET_MODE,
        &mode
    );
}


static void SWD_PHY_LineReset(void)
{
    /*
     * SWD line reset:
     * - Configure SWDIO as output.
     * - Hold SWDIO HIGH.
     * - Generate at least 50 SWCLK cycles.
     *
     * This implementation is blocking and is intended only
     * for initial PHY bring-up/testing.
     */

    SWD_PHY_SetSWDIOOutput();
    SWD_PHY_WriteSWDIO(true);
    SWD_PHY_WriteSWCLK(false);

    for (uint32_t i = 0U; i < SWD_PHY_LINE_RESET_CYCLES; i++)
    {
        SWD_PHY_WriteSWCLK(true);
        SWD_PHY_WriteSWCLK(false);
    }

    /* Leave the SWD bus in its idle state. */
    SWD_PHY_WriteSWDIO(true);
}



/*
 * Host -> target bit.
 * Data is placed while the clock is low; the target samples it on the
 * rising edge. The clock is left low afterwards.
 */
static void SWD_PHY_WriteBit(bool bit)
{
    SWD_PHY_WriteSWCLK(false);
    SWD_PHY_WriteSWDIO(bit);
    SWD_PHY_TestDelay();

    SWD_PHY_WriteSWCLK(true);
    SWD_PHY_TestDelay();

    SWD_PHY_WriteSWCLK(false);
}


/*
 * Target -> host bit.
 * The target changes SWDIO after the rising edge, so the host samples at
 * the end of the clock-low phase (just before the next rising edge).
 */
static bool SWD_PHY_ReadBit(void)
{
    bool bit;

    SWD_PHY_WriteSWCLK(false);
    SWD_PHY_TestDelay();

    bit = SWD_PHY_ReadSWDIO();

    SWD_PHY_WriteSWCLK(true);
    SWD_PHY_TestDelay();

    SWD_PHY_WriteSWCLK(false);

    return bit;
}


/* Clock out idle cycles with SWDIO low. The host must be driving SWDIO. */
static void SWD_PHY_Idle(uint8_t cycles)
{
    for (uint8_t i = 0U; i < cycles; i++)
    {
        SWD_PHY_WriteBit(false);
    }
}


/*
 * Host takes the line back after a target-driven phase: one turnaround
 * clock while SWDIO is still an input, then switch to output (low).
 */
static void SWD_PHY_TurnaroundToHost(void)
{
    (void)SWD_PHY_ReadBit();
    SWD_PHY_SetSWDIOOutput();
    SWD_PHY_WriteSWDIO(false);
}


static uint8_t SWD_PHY_Parity32(uint32_t value)
{
    uint8_t parity = 0U;

    for (uint8_t i = 0U; i < 32U; i++)
    {
        parity ^= (uint8_t)((value >> i) & 1U);
    }

    return parity;
}


/* --------------------------------------------------------------------------
 * Public functions
 * -------------------------------------------------------------------------- */

bool SWD_PHY_Init(void)
{
    /*
     * GPIO_Init() configures SWD_SWCLK and SWD_SWDIO
     * according to the GPIO initialization table.
     *
     * Assumes the application has already called GPIO_Init()
     * and both SWD pins are mapped in the current build.
     */

    GPIO_Write(SWD_SWCLK, GPIO_LOW);
    GPIO_Write(SWD_SWDIO, GPIO_HIGH);

    return true;
}


