
/**
 * @file    swd_test.c
 * @brief   Initial SWD communication test.
 *
 * @details Resets the SWD line and attempts to read the target DP IDCODE.
 */

#include "swd_test.h"
#include "swd_protocol.h"
#include "swd_phy.h"
#include "gpio/gpio.h"
#include "logger/frontend/logger.h"

bool SWD_Test_Run(void)
{
    uint32_t idcode = 0U;
    SWD_Status_t status;
    bool swdio_level;

    LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Starting communication test");

    status = SWD_Protocol_Init();

    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,"SWD: Initialization failed");
        return false;
    }

    /* Temporary SWDIO line-release test */
    SWD_PHY_SetDataInput();
    SWD_PHY_WriteClock(false);

    swdio_level = SWD_PHY_ReadData();

    LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Released SWDIO = %u",
             (unsigned int)swdio_level);

    /* Restore host control before protocol test */
    SWD_PHY_SetDataOutput();
    SWD_PHY_WriteData(true);

    status = SWD_Protocol_ReadIDCODE(&idcode);

    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,"SWD: DP IDCODE read failed");
        LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Status = %u", (unsigned int)status);
        return false;
    }

    LOG_INFO(LOG_MODULE_SYSTEM,"SWD: Target IDCODE = 0x%X",
             (unsigned long)idcode);

    return true;
}