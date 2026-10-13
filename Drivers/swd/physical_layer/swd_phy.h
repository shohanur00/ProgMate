/**
 * @file    swd_phy.h
 * @brief   SWD physical-layer interface.
 *
 * @details Provides low-level SWCLK and SWDIO operations for
 *          communication with an SWD-compatible target.
 *
 * @target  STM32G431CBT6
 */

#ifndef SWD_PHY_H
#define SWD_PHY_H

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialize SWD GPIO pins.
 *
 * @return true if initialization succeeds; otherwise false.
 */
bool SWD_PHY_Init(void);


/**
 * @brief Drive SWCLK to the requested logic level.
 *
 * @param state true = HIGH, false = LOW.
 */
void SWD_PHY_WriteSWCLK(bool state);


/**
 * @brief Drive SWDIO to the requested logic level.
 *
 * @param state true = HIGH, false = LOW.
 */
void SWD_PHY_WriteSWDIO(bool state);


/**
 * @brief Read the current SWDIO logic level.
 *
 * @return true if HIGH; otherwise false.
 */
bool SWD_PHY_ReadSWDIO(void);


/**
 * @brief Configure SWDIO as an output.
 */
void SWD_PHY_SetSWDIOOutput(void);


/**
 * @brief Configure SWDIO as an input.
 */
void SWD_PHY_SetSWDIOInput(void);


/**
 * @brief Generate the SWD line-reset sequence.
 *
 * @note Requires at least 50 SWCLK cycles with SWDIO HIGH.
 */
void SWD_PHY_LineReset(void);

#endif /* SWD_PHY_H */