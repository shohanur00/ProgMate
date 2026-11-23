#ifndef SWD_MEM_H
#define SWD_MEM_H

#include <stdint.h>
#include "../swd_types.h"

/*
 * MEM-AP memory access (32-bit, no auto-increment).
 * Precondition: DPIDR read and Debug/System power-up already done.
 */

/* Select AP bank 0 and configure CSW for 32-bit access. Call once per connection. */
SWD_Status_t SWD_Mem_Init(void);

SWD_Status_t SWD_Mem_Read32(uint32_t address, uint32_t *value);
SWD_Status_t SWD_Mem_Write32(uint32_t address, uint32_t value);

#endif /* SWD_MEM_H */