#ifndef SWD_FLASH_H
#define SWD_FLASH_H

#include <stdint.h>
#include "../swd_types.h"

/*
 * STM32G0 Flash driver over MEM-AP.
 * Precondition: DPIDR read, power-up, SWD_Mem_Init() done, core halted.
 */

SWD_Status_t SWD_Flash_Unlock(void);
SWD_Status_t SWD_Flash_Lock(void);

/* Mass erase (Flash must be unlocked). */
SWD_Status_t SWD_Flash_MassErase(void);

/* address 8-byte aligned, words must be even. Flash must be unlocked and blank. */
SWD_Status_t SWD_Flash_Program(uint32_t address, const uint32_t *data, uint32_t words);

/* *mismatches = number of words that differ from data. */
SWD_Status_t SWD_Flash_Verify(uint32_t address, const uint32_t *data,
                              uint32_t words, uint32_t *mismatches);

/* *non_blank = number of words that are not 0xFFFFFFFF. */
SWD_Status_t SWD_Flash_BlankCheck(uint32_t address, uint32_t words, uint32_t *non_blank);

#endif /* SWD_FLASH_H */