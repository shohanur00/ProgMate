/**
 * @file    swd_phy.c
 * @brief   SWD physical-layer implementation.
 *
 * @target  STM32G431CBT6
 *
 * @details Controls SWCLK and SWDIO through the GPIO driver.
 *          Hardware only: pins, clock timing, bit order, SWDIO direction.
 */

#include "swd_phy.h"

#include "gpio/gpio.h"
#include "cdefs/cdefs.h"

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

#define SWD_PHY_LINE_RESET_CYCLES    (56U)

/*
 * Half clock period delay (busy-loop iterations).
 * Tune with a scope or logic analyzer.
 */
#ifndef SWD_PHY_HALF_PERIOD_LOOPS
#define SWD_PHY_HALF_PERIOD_LOOPS    50U
#endif

/* --------------------------------------------------------------------------
 * Private helpers
 * -------------------------------------------------------------------------- */

static void SWD_PHY_Delay(void)
{
    for (volatile uint32_t i = 0U; i < SWD_PHY_HALF_PERIOD_LOOPS; i++)
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


/*
 * Make SWDIO an output. The output latch is loaded with 'level' first,
 * so the pin starts at the requested level when the driver turns on.
 */
static void SWD_PHY_SetSWDIOOutput(bool level)
{
    GPIO_Mode_t mode = GPIO_MODE_OUTPUT;

    GPIO_Write(SWD_SWDIO, level ? GPIO_HIGH : GPIO_LOW);
    GPIO_Ioctl(SWD_SWDIO, GPIO_IOCTL_SET_MODE, &mode);
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


/*
 * Host -> target bit.
 * Data is placed while the clock is low; the target samples it on the
 * rising edge. The clock is left low afterwards.
 */
static void SWD_PHY_WriteBit(bool bit)
{
    SWD_PHY_WriteSWCLK(false);
    SWD_PHY_WriteSWDIO(bit);
    SWD_PHY_Delay();

    SWD_PHY_WriteSWCLK(true);
    SWD_PHY_Delay();

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
    SWD_PHY_Delay();

    bit = SWD_PHY_ReadSWDIO();

    SWD_PHY_WriteSWCLK(true);
    SWD_PHY_Delay();

    SWD_PHY_WriteSWCLK(false);

    return bit;
}


/* One full clock cycle: low -> high -> low. SWDIO is not touched. */
static void SWD_PHY_ClockPulse(void)
{
    SWD_PHY_WriteSWCLK(false);
    SWD_PHY_Delay();

    SWD_PHY_WriteSWCLK(true);
    SWD_PHY_Delay();

    SWD_PHY_WriteSWCLK(false);
}


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

bool SWD_PHY_Init(void)
{
    SWD_PHY_WriteSWCLK(false);
    SWD_PHY_SetSWDIOOutput(true);       /* idle level: SWDIO high */

    return true;
}


/*
 * Hand SWDIO over from host to target.
 * The host stops driving first, then one turnaround clock is issued
 * while nobody drives the line. The target starts driving after it.
 */
void SWD_PHY_TurnaroundToTarget(void)
{
    SWD_PHY_SetSWDIOInput();            /* release SWDIO before the clock   */
    SWD_PHY_ClockPulse();               /* turnaround cycle                 */
}


/*
 * Take SWDIO back from the target.
 * One turnaround clock is issued while SWDIO is still an input, then
 * the host drives the line low (idle level).
 */
void SWD_PHY_TurnaroundToHost(void)
{
    SWD_PHY_ClockPulse();               /* turnaround cycle                 */
    SWD_PHY_SetSWDIOOutput(false);      /* host drives only after the clock */
}


/*
 * Drive 'count' bits (1..32) on SWDIO, LSB first.
 * Precondition: the host owns SWDIO (output mode).
 */
void SWD_PHY_WriteBits(uint32_t value, uint8_t count)
{
    for (uint8_t i = 0U; i < count; i++)
    {
        SWD_PHY_WriteBit(((value >> i) & 1U) != 0U);
    }
}


/*
 * Sample 'count' bits (1..32) from SWDIO, LSB first.
 * Precondition: the target owns SWDIO (host in input mode).
 */
uint32_t SWD_PHY_ReadBits(uint8_t count)
{
    uint32_t value = 0U;

    for (uint8_t i = 0U; i < count; i++)
    {
        value |= ((uint32_t)SWD_PHY_ReadBit() << i);
    }

    return value;
}


/*
 * Line reset: SWDIO high for SWD_PHY_LINE_RESET_CYCLES clocks.
 * Note: swd_protocol.c already sends a line reset with WriteBits(), so
 * this function is only needed if the protocol layer is changed to call it.
 */
void SWD_PHY_LineReset(void)
{
    SWD_PHY_SetSWDIOOutput(true);

    for (uint32_t i = 0U; i < SWD_PHY_LINE_RESET_CYCLES; i++)
    {
        SWD_PHY_ClockPulse();           /* same timing as every other bit   */
    }
}