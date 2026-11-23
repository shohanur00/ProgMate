#include <stdint.h>
#include <stddef.h>

#include "swd_mem.h"
#include "../swd.h"


#define DP_REG_SELECT    0x08U
#define DP_REG_RDBUFF    0x0CU

#define AP_REG_CSW       0x00U
#define AP_REG_TAR       0x04U
#define AP_REG_DRW       0x0CU

/* 32-bit access, no auto-increment. Prot bits as commonly used for Cortex-M. */
#define MEMAP_CSW_32BIT  0x23000002UL


SWD_Status_t SWD_Mem_Init(void)
{
    SWD_Status_t status;
    uint32_t csw = MEMAP_CSW_32BIT;

    /* APSEL = 0, APBANKSEL = 0 (CSW, TAR, DRW are all in bank 0). */
    status = SWD_WriteDPRegister(DP_REG_SELECT, 0x00000000UL);
    if (status != SWD_STATUS_OK)
    {
        return status;
    }

    return SWD_Transfer(true, false, AP_REG_CSW, &csw);
}


SWD_Status_t SWD_Mem_Read32(uint32_t address, uint32_t *value)
{
    SWD_Status_t status;
    uint32_t tar   = address;
    uint32_t dummy = 0U;

    if (value == NULL)
    {
        return SWD_STATUS_INVALID_PARAM;
    }

    status = SWD_Transfer(true, false, AP_REG_TAR, &tar);
    if (status != SWD_STATUS_OK) { return status; }

    /* DRW read is posted: returns stale data. */
    status = SWD_Transfer(true, true, AP_REG_DRW, &dummy);
    if (status != SWD_STATUS_OK) { return status; }

    /* RDBUFF returns the real value. */
    return SWD_ReadDPRegister(DP_REG_RDBUFF, value);
}


SWD_Status_t SWD_Mem_Write32(uint32_t address, uint32_t value)
{
    SWD_Status_t status;
    uint32_t tar   = address;
    uint32_t data  = value;
    uint32_t dummy = 0U;

    status = SWD_Transfer(true, false, AP_REG_TAR, &tar);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Transfer(true, false, AP_REG_DRW, &data);
    if (status != SWD_STATUS_OK) { return status; }

    /* Writes are posted too: reading RDBUFF waits for the write and reports its status. */
    return SWD_ReadDPRegister(DP_REG_RDBUFF, &dummy);
}