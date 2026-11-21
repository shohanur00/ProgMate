#ifndef SWD_H
#define SWD_H

#include <stdint.h>
#include <stdbool.h>

#include "swd_types.h"

/*
 * SWD transfer engine.
 *
 * Connects swd_protocol (packet building and decoding) with swd_phy
 * (bits on the wire): it sends the packets built by the protocol layer
 * through the PHY, collects the answer, and hands it back to the protocol
 * layer to decode. It also applies the WAIT retry and FAULT recovery policy.
 */

/* Initialize the PHY. */
SWD_Status_t SWD_Init(void);

/* Line reset, JTAG-to-SWD switch, line reset, idle (sequence from swd_protocol). */
SWD_Status_t SWD_Connect(void);

/*
 * One full transaction with WAIT retry and FAULT recovery.
 *
 *   ap   : false = DP register, true = AP register
 *   read : true = read, false = write
 *   addr : register address, only bits [3:2] go on the wire
 *   data : read -> receives the value, write -> holds the value to send
 */
SWD_Status_t SWD_Transfer(bool ap, bool read, uint8_t addr, uint32_t *data);


/**
 * @brief Read the Debug Port Identification Register (DPIDR).
 *
 * @param idcode Pointer to store the 32-bit DP IDCODE.
 *
 * @return SWD_STATUS_OK on success, otherwise an error status.
 */
SWD_Status_t SWD_ReadDPIDCODE(uint32_t *idcode);


/**
 * @brief Read a Debug Port register.
 *
 * @param address DP register address.
 * @param data    Pointer to store the register value.
 *
 * @return SWD_STATUS_OK on success, otherwise an error status.
 */
SWD_Status_t SWD_ReadDPRegister(uint8_t address, uint32_t *data);


/**
 * @brief Write a Debug Port register.
 *
 * @param address DP register address.
 * @param data    Value to write.
 *
 * @return SWD_STATUS_OK on success, otherwise an error status.
 */
SWD_Status_t SWD_WriteDPRegister(uint8_t address, uint32_t data);


/**
 * @brief Read the Debug Port Identification Register.
 *
 * @param idcode Pointer to store the DP IDCODE.
 *
 * @return SWD_STATUS_OK on success, otherwise an error status.
 */
SWD_Status_t SWD_ReadDPIDCODE(uint32_t *idcode);


/**
 * @brief Request the Debug System Power.
 *
 * This function requests the target to enter debug system power mode.
 *
 * @return SWD_STATUS_OK on success, otherwise an error status.
 */

SWD_Status_t SWD_RequestDebugSystemPower(void);


#endif /* SWD_H */