/**
 * @file    swd_phy.h
 * @brief   SWD physical layer (hardware only).
 *
 * @details This layer knows pins, clock timing, bit order and SWDIO
 *          direction. It knows nothing about requests, ACK values,
 *          parity or registers. Everything protocol related lives in
 *          swd_protocol.
 */

#ifndef SWD_PHY_H
#define SWD_PHY_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Configure the idle state: SWCLK low, SWDIO output high.
 * Assumes GPIO_Init() has already configured the SWD pins.
 * Returns true on success.
 */
bool SWD_PHY_Init(void);

/*
 * Drive 'count' bits (1..32) on SWDIO, LSB first, one SWCLK cycle per bit.
 * Precondition: the host owns SWDIO (output mode).
 */
void SWD_PHY_WriteBits(uint32_t value, uint8_t count);

/*
 * Sample 'count' bits (1..32) from SWDIO, LSB first, one SWCLK cycle per bit.
 * Precondition: SWDIO is released (see SWD_PHY_TurnaroundToTarget).
 */
uint32_t SWD_PHY_ReadBits(uint8_t count);

/*
 * Host releases SWDIO (input), then one SWCLK cycle is issued while
 * nobody drives the line.
 */
void SWD_PHY_TurnaroundToTarget(void);

/*
 * One SWCLK cycle while SWDIO is still an input, then the host drives
 * SWDIO low (output).
 */
void SWD_PHY_TurnaroundToHost(void);

/*
 * SWDIO high for SWD_PHY_LINE_RESET_CYCLES clocks.
 * Optional: swd_protocol currently sends its own line reset using
 * SWD_PHY_WriteBits(), so nothing calls this function.
 */
void SWD_PHY_LineReset(void);

#endif /* SWD_PHY_H */