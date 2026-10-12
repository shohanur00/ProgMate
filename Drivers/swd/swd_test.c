
/**
 * @file    swd_test.c
 * @brief   Initial SWD communication test.
 *
 * @details Resets the SWD line and attempts to read the target DP IDCODE.
 */

#include "swd_test.h"

#include "swd_protocol.h"
#include "logger/frontend/logger.h"

bool SWD_Test_Run(void)
{
    uint32_t idcode = 0U;
    SWD_Status_t status;

    LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Starting communication test");

    status = SWD_Protocol_ReadIDCODE(&idcode);

    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,"SWD: DP IDCODE read failed");
        LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Status = %u", (unsigned int)status);
        return false;
    }

    status = SWD_Protocol_ReadIDCODE(&idcode);

    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,"SWD: DP IDCODE read failed");
        return false;
    }

    LOG_INFO(LOG_MODULE_SYSTEM,"SWD: DP IDCODE read successful");
    LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Target IDCODE = 0x%X", (unsigned long)idcode);

    return true;
}