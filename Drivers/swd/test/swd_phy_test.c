#include <stdint.h>
#include <stdbool.h>

#include "logger/frontend/logger.h"
#include "../swd.h"
#include "../protocol/swd_protocol.h"
#include "../swd_types.h"
#include "swd_test.h"


/* --------------------------------------------------------------------------
 * Private helpers
 * -------------------------------------------------------------------------- */

static const char *SWD_TestStatusName(SWD_Status_t status)
{
    switch (status)
    {
        case SWD_STATUS_OK:       return "OK";
        case SWD_STATUS_WAIT:     return "WAIT";
        case SWD_STATUS_FAULT:    return "FAULT";
        case SWD_STATUS_NO_ACK:   return "NO_ACK (check wiring, GND, power, NRST)";
        case SWD_STATUS_PARITY:   return "PARITY";
        case SWD_STATUS_TIMEOUT:  return "TIMEOUT";
        case SWD_STATUS_PARAM:    return "PARAM";
        case SWD_STATUS_HW_ERROR: return "HW_ERROR";
        default:                  return "UNKNOWN";
    }
}


/* Returns 1 on pass, 0 on fail, and logs the failure. */
static uint8_t SWD_TestCheckRequest(const char *name,
                                    bool ap, bool read, uint8_t addr,
                                    uint8_t expected)
{
    SWD_TxPacket_t tx;
    uint8_t request;

    (void)SWD_Protocol_BuildPacket(ap, read, addr, 0U, &tx);
    request = SWD_Protocol_PackRequest(&tx.request);

    if (request != expected)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "SWD test: %s request 0x%X, expected 0x%X",
                  name, request, expected);
        return 0U;
    }

    return 1U;
}


static uint8_t SWD_TestCheckDecode(const char *name,
                                   uint8_t ack, uint32_t data, uint8_t parity,
                                   SWD_Status_t expected)
{
    SWD_RxPacket_t rx;
    uint32_t value = 0U;
    SWD_Status_t status;

    rx.ack         = ack;
    rx.data        = data;
    rx.data_parity = parity;

    status = SWD_Protocol_DecodePacket(true, &rx, &value);

    if (status != expected)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "SWD test: %s decoded as %s", name, SWD_TestStatusName(status));
        return 0U;
    }

    return 1U;
}


/* --------------------------------------------------------------------------
 * Public tests
 * -------------------------------------------------------------------------- */

void SWD_TestProtocolPackets(void)
{
    uint8_t passed = 0U;
    const uint8_t total = 9U;

    /* Request words (hand-calculated). */
    passed += SWD_TestCheckRequest("DP read DPIDR",     false, true,  0x00U, 0xA5U);
    passed += SWD_TestCheckRequest("DP write CTRLSTAT", false, false, 0x04U, 0xA9U);
    passed += SWD_TestCheckRequest("DP write SELECT",   false, false, 0x08U, 0xB1U);
    passed += SWD_TestCheckRequest("DP read RDBUFF",    false, true,  0x0CU, 0xBDU);
    passed += SWD_TestCheckRequest("AP read IDR",       true,  true,  0xFCU, 0x9FU);

    /* 0x0BC11477 has 14 bits set, so its even-parity bit is 0. */
    passed += SWD_TestCheckDecode("ACK OK, good parity", 0x01U, 0x0BC11477UL, 0U, SWD_STATUS_OK);
    passed += SWD_TestCheckDecode("ACK OK, bad parity",  0x01U, 0x0BC11477UL, 1U, SWD_STATUS_PARITY);
    passed += SWD_TestCheckDecode("ACK WAIT",            0x02U, 0U,           0U, SWD_STATUS_WAIT);
    passed += SWD_TestCheckDecode("ACK FAULT",           0x04U, 0U,           0U, SWD_STATUS_FAULT);

    if (passed == total)
    {
        LOG_INFO(LOG_MODULE_SYSTEM,
                 "SWD test: packet tests passed (%d/%d)", (int)passed, (int)total);
    }
    else
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "SWD test: packet tests FAILED (%d/%d passed)", (int)passed, (int)total);
    }
}


void SWD_TestReadDPIDCODE(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;

    LOG_INFO(LOG_MODULE_SYSTEM, "SWD: Starting DP IDCODE read");

    status = SWD_Init();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "SWD: Init failed: %s", SWD_TestStatusName(status));
        return;
    }

    status = SWD_Connect();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "SWD: Connect failed: %s", SWD_TestStatusName(status));
        return;
    }

    /* DP register 0x00 read = DPIDR. */
    status = SWD_Transfer(false, true, 0x00U, &idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SYSTEM,
                  "SWD: DPIDR read failed: %s", SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SYSTEM, "SWD: DP IDCODE = 0x%X", idcode);
}