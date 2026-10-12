/**
 * @file    swd_protocol.h
 * @brief   SWD protocol-layer interface.
 *
 * @target  STM32G431CBT6
 */

#ifndef SWD_PROTOCOL_H
#define SWD_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * SWD ACK responses
 * -------------------------------------------------------------------------- */

typedef enum
{
    SWD_ACK_OK       = 0x01U,
    SWD_ACK_WAIT     = 0x02U,
    SWD_ACK_FAULT    = 0x04U,
    SWD_ACK_INVALID  = 0xFFU

} SWD_ACK_t;


/* --------------------------------------------------------------------------
 * SWD status
 * -------------------------------------------------------------------------- */

typedef enum
{
    SWD_STATUS_OK = 0U,
    SWD_STATUS_ERROR,
    SWD_STATUS_TIMEOUT,
    SWD_STATUS_WAIT,
    SWD_STATUS_FAULT,
    SWD_STATUS_PARITY_ERROR,
    SWD_STATUS_INVALID_ARGUMENT

} SWD_Status_t;


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialize the SWD protocol layer.
 *
 * @return SWD_STATUS_OK on success.
 */
SWD_Status_t SWD_Protocol_Init(void);


/**
 * @brief Send an SWD line-reset sequence.
 *
 * @return SWD_STATUS_OK on success.
 */
SWD_Status_t SWD_Protocol_LineReset(void);


/**
 * @brief Read the Debug Port IDCODE register.
 *
 * @param idcode Pointer to receive the 32-bit IDCODE.
 * @return SWD_STATUS_OK on success.
 */
SWD_Status_t SWD_Protocol_ReadIDCODE(uint32_t *idcode);


/**
 * @brief Read a Debug Port register.
 *
 * @param address Register address bits A[3:2].
 * @param data Pointer to receive the register value.
 * @return Transaction status.
 */
SWD_Status_t SWD_Protocol_DP_Read(uint8_t address, uint32_t *data);


/**
 * @brief Write a Debug Port register.
 *
 * @param address Register address bits A[3:2].
 * @param data 32-bit value to write.
 * @return Transaction status.
 */
SWD_Status_t SWD_Protocol_DP_Write(uint8_t address, uint32_t data);


/**
 * @brief Read an Access Port register.
 *
 * @param ap_number Access Port number.
 * @param address Register address bits A[3:2].
 * @param data Pointer to receive the register value.
 * @return Transaction status.
 */
SWD_Status_t SWD_Protocol_AP_Read(
    uint8_t ap_number,
    uint8_t address,
    uint32_t *data
);


/**
 * @brief Write an Access Port register.
 *
 * @param ap_number Access Port number.
 * @param address Register address bits A[3:2].
 * @param data 32-bit value to write.
 * @return Transaction status.
 */
SWD_Status_t SWD_Protocol_AP_Write(
    uint8_t ap_number,
    uint8_t address,
    uint32_t data
);

#endif /* SWD_PROTOCOL_H */