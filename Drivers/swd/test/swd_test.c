
#include <stdint.h>

#include "../swd.h"
#include "swd_test.h"

#include "logger/frontend/logger.h"


/* --------------------------------------------------------------------------
 * Test: Read DP IDCODE
 * -------------------------------------------------------------------------- */


static const char *SWD_TestStatusName(SWD_Status_t status)
{
    switch (status)
    {
        case SWD_STATUS_OK:
            return "OK";

        case SWD_STATUS_INVALID_PARAM:
            return "INVALID_PARAM";

        default:
            return "UNKNOWN";
    }
}


void SWD_TestReadDPIDCODE(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting DP IDCODE test");

    status = SWD_Init();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    status = SWD_Connect();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: DPIDR read failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SWD,
             "SWD: DP IDCODE = 0x%X",
             (unsigned long)idcode);
}


/* --------------------------------------------------------------------------
 * Test: DP Register Read
 * -------------------------------------------------------------------------- */

void SWD_TestDPRegisters(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;
    uint32_t ctrl_stat = 0U;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting DP register test");

    status = SWD_Init();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    status = SWD_Connect();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    /* Read DPIDR */
    status = SWD_ReadDPRegister(0x00U, &idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: DPIDR read failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SWD,
             "SWD: DPIDR = 0x%X",
             (unsigned long)idcode);

    /* Read CTRL/STAT */
    status = SWD_ReadDPRegister(0x04U, &ctrl_stat);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: CTRL/STAT read failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SWD,
             "SWD: CTRL/STAT = 0x%X",
             (unsigned long)ctrl_stat);

    LOG_INFO(LOG_MODULE_SWD, "SWD: DP register test passed");
}
