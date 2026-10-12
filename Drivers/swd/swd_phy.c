
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

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

#define SWD_PHY_LINE_RESET_CYCLES    (56U)

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


void SWD_PHY_WriteClock(bool state)
{
    GPIO_Write(
        SWD_SWCLK,
        (state == true) ? GPIO_HIGH : GPIO_LOW
    );
}


void SWD_PHY_WriteData(bool state)
{
    GPIO_Write(
        SWD_SWDIO,
        (state == true) ? GPIO_HIGH : GPIO_LOW
    );
}


bool SWD_PHY_ReadData(void)
{
    return (GPIO_Read(SWD_SWDIO) == GPIO_HIGH);
}


void SWD_PHY_SetDataOutput(void)
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


void SWD_PHY_SetDataInput(void)
{
    GPIO_Mode_t mode = GPIO_MODE_INPUT;

    GPIO_Ioctl(
        SWD_SWDIO,
        GPIO_IOCTL_SET_MODE,
        &mode
    );
}


void SWD_PHY_LineReset(void)
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

    SWD_PHY_SetDataOutput();
    SWD_PHY_WriteData(true);
    SWD_PHY_WriteClock(false);

    for (uint32_t i = 0U; i < SWD_PHY_LINE_RESET_CYCLES; i++)
    {
        SWD_PHY_WriteClock(true);
        SWD_PHY_WriteClock(false);
    }

    /* Leave the SWD bus in its idle state. */
    SWD_PHY_WriteData(true);
}