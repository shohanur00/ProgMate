
#include <stdint.h>

#include "../swd.h"
#include "swd_test.h"
#include "../mem/swd_mem.h"

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

        case SWD_STATUS_WAIT:
            return "WAIT";

        case SWD_STATUS_FAULT:
            return "FAULT";

        case SWD_STATUS_NO_ACK:
            return "NO_ACK";

        case SWD_STATUS_PARITY:
            return "PARITY";

        case SWD_STATUS_TIMEOUT:
            return "TIMEOUT";

        case SWD_STATUS_PARAM:
            return "PARAM";

        case SWD_STATUS_HW_ERROR:
            return "HW_ERROR";

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



void SWD_TestDebugSystemPower(void)
{
    SWD_Status_t status;
    uint32_t idcode    = 0U;
    uint32_t ctrl_stat = 0U;

    LOG_INFO(LOG_MODULE_SWD,
             "SWD: Requesting Debug/System power");

    status = SWD_Init();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD,
                  "SWD: Init failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    status = SWD_Connect();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD,
                  "SWD: Connect failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    /* Step 1: the first transaction after a line reset must be a DPIDR read. */
    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD,
                  "SWD: Step 1 DPIDR read failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SWD,
             "SWD: Step 1 DPIDR = 0x%X",
             (unsigned long)idcode);

    /* Step 2: request Debug and System power-up (CDBGPWRUPREQ | CSYSPWRUPREQ). */
    status = SWD_WriteDPRegister(0x04U, (1UL << 28U) | (1UL << 30U));
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD,
                  "SWD: Step 2 CTRL/STAT write failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SWD, "SWD: Step 2 CTRL/STAT write OK");

    /* Step 3: poll CTRL/STAT until CDBGPWRUPACK and CSYSPWRUPACK are set. */
    for (uint32_t i = 0U; i < 100U; i++)
    {
        status = SWD_ReadDPRegister(0x04U, &ctrl_stat);
        if (status != SWD_STATUS_OK)
        {
            LOG_ERROR(LOG_MODULE_SWD,
                      "SWD: Step 3 CTRL/STAT read failed (poll %d): %s",
                      (int)i,
                      SWD_TestStatusName(status));
            return;
        }

        if ((ctrl_stat & ((1UL << 29U) | (1UL << 31U))) ==
            ((1UL << 29U) | (1UL << 31U)))
        {
            LOG_INFO(LOG_MODULE_SWD,
                     "SWD: Debug/System power-up successful, CTRL/STAT = 0x%X (poll %d)",
                     (unsigned long)ctrl_stat,
                     (int)i);
            return;
        }
    }

    LOG_ERROR(LOG_MODULE_SWD,
              "SWD: Step 3 timeout, last CTRL/STAT = 0x%X",
              (unsigned long)ctrl_stat);
}


void SWD_TestReadAPIDR(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;
    uint32_t dummy  = 0U;
    uint32_t apidr  = 0U;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting AP IDR test");

    if (SWD_Init() != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed");
        return;
    }

    if (SWD_Connect() != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed");
        return;
    }

    /* Step 1: DPIDR must be the first transaction after line reset. */
    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Step 1 DPIDR failed: %s",
                  SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: Step 1 DPIDR = 0x%X", (unsigned long)idcode);

    /* Step 2: Debug/System power-up (now safe, DPIDR was read first). */
    status = SWD_RequestDebugSystemPower();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Step 2 power-up failed: %s",
                  SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: Step 2 power-up OK");

    /* Step 3: DP SELECT: APSEL = 0, APBANKSEL = 0xF, DPBANKSEL = 0. */
    status = SWD_WriteDPRegister(0x08U, 0x000000F0UL);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Step 3 SELECT write failed: %s",
                  SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: Step 3 SELECT write OK");

    /* Step 4: AP read is posted, so this returns stale data (discarded). */
    status = SWD_Transfer(true, true, 0xFCU, &dummy);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Step 4 AP IDR read failed: %s",
                  SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: Step 4 AP IDR read issued");

    /* Step 5: RDBUFF (DP 0x0C) returns the AP IDR value. */
    status = SWD_ReadDPRegister(0x0CU, &apidr);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Step 5 RDBUFF read failed: %s",
                  SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SWD, "SWD: AP IDR = 0x%X", (unsigned long)apidr);
}

/* --------------------------------------------------------------------------
 * Test: MEM-AP read (read-only)
 * -------------------------------------------------------------------------- */

#define AP_REG_CSW      0x00U
#define AP_REG_TAR      0x04U
#define AP_REG_DRW      0x0CU
#define DP_REG_SELECT   0x08U
#define DP_REG_RDBUFF   0x0CU

/* 32-bit access, no auto-increment. Prot bits as commonly used for Cortex-M. */
#define MEMAP_CSW_32BIT 0x23000002UL

static SWD_Status_t SWD_TestMemRead32(uint32_t address, uint32_t *value)
{
    SWD_Status_t status;
    uint32_t csw   = MEMAP_CSW_32BIT;
    uint32_t tar   = address;
    uint32_t dummy = 0U;

    status = SWD_Transfer(true, false, AP_REG_CSW, &csw);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Transfer(true, false, AP_REG_TAR, &tar);
    if (status != SWD_STATUS_OK) { return status; }

    /* DRW read is posted: this returns stale data. */
    status = SWD_Transfer(true, true, AP_REG_DRW, &dummy);
    if (status != SWD_STATUS_OK) { return status; }

    /* RDBUFF returns the real value. */
    return SWD_ReadDPRegister(DP_REG_RDBUFF, value);
}


void SWD_TestMemAP(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;
    uint32_t value  = 0U;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting MEM-AP read test");

    if (SWD_Init() != SWD_STATUS_OK)    { LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed");    return; }
    if (SWD_Connect() != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed"); return; }

    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: DPIDR failed: %s", SWD_TestStatusName(status));
        return;
    }

    status = SWD_RequestDebugSystemPower();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Power-up failed: %s", SWD_TestStatusName(status));
        return;
    }

    /* APSEL = 0, APBANKSEL = 0 (CSW/TAR/DRW are all in bank 0). */
    status = SWD_WriteDPRegister(DP_REG_SELECT, 0x00000000UL);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: SELECT failed: %s", SWD_TestStatusName(status));
        return;
    }

    /* Cortex-M CPUID (architectural register). */
    status = SWD_TestMemRead32(0xE000ED00UL, &value);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: CPUID read failed: %s", SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: CPUID = 0x%X", (unsigned long)value);

    /* Flash start: initial stack pointer of the target firmware. */
    status = SWD_TestMemRead32(0x08000000UL, &value);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Flash read failed: %s", SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: [0x08000000] = 0x%X", (unsigned long)value);

    /* DBGMCU_IDCODE (STM32G0 address, unverified). */
    status = SWD_TestMemRead32(0x40015800UL, &value);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: DBGMCU read failed: %s", SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: DBGMCU_IDCODE = 0x%X", (unsigned long)value);
}


#define CORTEX_DHCSR          0xE000EDF0UL
#define DHCSR_HALT_ENABLE     0xA05F0003UL   /* DBGKEY | C_HALT | C_DEBUGEN */
#define DHCSR_RESUME          0xA05F0000UL   /* DBGKEY, halt and debug-enable cleared */
#define DHCSR_S_HALT          (1UL << 17U)

#define SRAM_TEST_ADDRESS     0x20000000UL
#define SRAM_TEST_PATTERN     0xA5A55A5AUL


void SWD_TestMemWrite(void)
{
    SWD_Status_t status;
    uint32_t idcode   = 0U;
    uint32_t dhcsr    = 0U;
    uint32_t original = 0U;
    uint32_t readback = 0U;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting memory write test");

    if (SWD_Init() != SWD_STATUS_OK)    { LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed");    return; }
    if (SWD_Connect() != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed"); return; }

    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: DPIDR failed: %s", SWD_TestStatusName(status));
        return;
    }

    status = SWD_RequestDebugSystemPower();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Power-up failed: %s", SWD_TestStatusName(status));
        return;
    }

    status = SWD_Mem_Init();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: MEM-AP init failed: %s", SWD_TestStatusName(status));
        return;
    }

    /* Halt the core. */
    status = SWD_Mem_Write32(CORTEX_DHCSR, DHCSR_HALT_ENABLE);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: DHCSR halt write failed: %s", SWD_TestStatusName(status));
        return;
    }

    status = SWD_Mem_Read32(CORTEX_DHCSR, &dhcsr);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: DHCSR read failed: %s", SWD_TestStatusName(status));
        return;
    }
    LOG_INFO(LOG_MODULE_SWD, "SWD: DHCSR = 0x%X, halted = %d",
             (unsigned long)dhcsr, (int)((dhcsr & DHCSR_S_HALT) != 0U));

    /* SRAM write test: save, write pattern, read back, restore. */
    status = SWD_Mem_Read32(SRAM_TEST_ADDRESS, &original);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: SRAM read failed: %s", SWD_TestStatusName(status));
        return;
    }

    status = SWD_Mem_Write32(SRAM_TEST_ADDRESS, SRAM_TEST_PATTERN);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: SRAM write failed: %s", SWD_TestStatusName(status));
        return;
    }

    status = SWD_Mem_Read32(SRAM_TEST_ADDRESS, &readback);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: SRAM readback failed: %s", SWD_TestStatusName(status));
        return;
    }

    LOG_INFO(LOG_MODULE_SWD, "SWD: SRAM wrote 0x%X, read 0x%X, %s",
             (unsigned long)SRAM_TEST_PATTERN,
             (unsigned long)readback,
             (readback == SRAM_TEST_PATTERN) ? "MATCH" : "MISMATCH");

    /* Restore the original word and let the core run again. */
    (void)SWD_Mem_Write32(SRAM_TEST_ADDRESS, original);
    (void)SWD_Mem_Write32(CORTEX_DHCSR, DHCSR_RESUME);
}





#define G0_FLASH_BASE        0x40022000UL
#define G0_FLASH_KEYR        (G0_FLASH_BASE + 0x08UL)
#define G0_FLASH_SR          (G0_FLASH_BASE + 0x10UL)
#define G0_FLASH_CR          (G0_FLASH_BASE + 0x14UL)

#define G0_FLASH_CR_LOCK     (1UL << 31U)
#define G0_FLASH_KEY1        0x45670123UL
#define G0_FLASH_KEY2        0xCDEF89ABUL

#define G0_FLASHSIZE_ADDR    0x1FFF75E0UL


void SWD_TestFlashUnlock(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;
    uint32_t value  = 0U;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting Flash unlock test");

    if (SWD_Init() != SWD_STATUS_OK)    { LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed");    return; }
    if (SWD_Connect() != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed"); return; }

    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: DPIDR failed: %s", SWD_TestStatusName(status)); return; }

    status = SWD_RequestDebugSystemPower();
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Power-up failed: %s", SWD_TestStatusName(status)); return; }

    status = SWD_Mem_Init();
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: MEM-AP init failed: %s", SWD_TestStatusName(status)); return; }

    /* Halt the core so the target firmware does not touch the Flash controller. */
    status = SWD_Mem_Write32(CORTEX_DHCSR, DHCSR_HALT_ENABLE);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Halt failed: %s", SWD_TestStatusName(status)); return; }

    /* Flash size in KB (low 7 bits of the register). */
    status = SWD_Mem_Read32(G0_FLASHSIZE_ADDR, &value);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Flash size read failed: %s", SWD_TestStatusName(status)); goto resume; }
    LOG_INFO(LOG_MODULE_SWD, "SWD: Flash size = %d KB", (int)(value & 0x7FUL));

    /* Status and control before unlock. */
    status = SWD_Mem_Read32(G0_FLASH_SR, &value);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_SR read failed: %s", SWD_TestStatusName(status)); goto resume; }
    LOG_INFO(LOG_MODULE_SWD, "SWD: FLASH_SR = 0x%X", (unsigned long)value);

    status = SWD_Mem_Read32(G0_FLASH_CR, &value);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_CR read failed: %s", SWD_TestStatusName(status)); goto resume; }
    LOG_INFO(LOG_MODULE_SWD, "SWD: FLASH_CR before unlock = 0x%X, locked = %d",
             (unsigned long)value, (int)((value & G0_FLASH_CR_LOCK) != 0U));

    /* Write the keys only if the Flash is locked. A wrong sequence locks it until reset. */
    if ((value & G0_FLASH_CR_LOCK) != 0U)
    {
        status = SWD_Mem_Write32(G0_FLASH_KEYR, G0_FLASH_KEY1);
        if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: KEY1 write failed: %s", SWD_TestStatusName(status)); goto resume; }

        status = SWD_Mem_Write32(G0_FLASH_KEYR, G0_FLASH_KEY2);
        if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: KEY2 write failed: %s", SWD_TestStatusName(status)); goto resume; }

        status = SWD_Mem_Read32(G0_FLASH_CR, &value);
        if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_CR read failed: %s", SWD_TestStatusName(status)); goto resume; }
    }

    LOG_INFO(LOG_MODULE_SWD, "SWD: FLASH_CR after unlock = 0x%X, locked = %d",
             (unsigned long)value, (int)((value & G0_FLASH_CR_LOCK) != 0U));

    /* Always lock again, whatever happened above. */
    status = SWD_Mem_Write32(G0_FLASH_CR, G0_FLASH_CR_LOCK);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Re-lock write failed: %s", SWD_TestStatusName(status)); goto resume; }

    status = SWD_Mem_Read32(G0_FLASH_CR, &value);
    if (status == SWD_STATUS_OK)
    {
        LOG_INFO(LOG_MODULE_SWD, "SWD: FLASH_CR after re-lock = 0x%X, locked = %d",
                 (unsigned long)value, (int)((value & G0_FLASH_CR_LOCK) != 0U));
    }

resume:
    (void)SWD_Mem_Write32(CORTEX_DHCSR, DHCSR_RESUME);
}








#define G0_FLASH_CR_MER1        (1UL << 2U)
#define G0_FLASH_CR_STRT        (1UL << 16U)

#define G0_FLASH_SR_EOP         (1UL << 0U)
#define G0_FLASH_SR_BSY1        (1UL << 16U)
#define G0_FLASH_SR_CFGBSY      (1UL << 18U)
/* OPERR, PROGERR, WRPERR, PGAERR, SIZERR, PGSERR, MISERR, FASTERR, OPTVERR */
#define G0_FLASH_SR_ERR_MASK    0x000083FAUL

#define G0_FLASH_START          0x08000000UL
#define G0_ERASE_POLL_LIMIT     2000U


/* Count words that are not 0xFFFFFFFF. Returns false on a read error. */
static bool SWD_TestCountNonBlank(uint32_t start, uint32_t words, uint32_t *bad)
{
    uint32_t value = 0U;

    *bad = 0U;

    for (uint32_t i = 0U; i < words; i++)
    {
        if (SWD_Mem_Read32(start + (i * 4UL), &value) != SWD_STATUS_OK)
        {
            return false;
        }

        if (value != 0xFFFFFFFFUL)
        {
            (*bad)++;
        }
    }

    return true;
}


void SWD_TestMassErase(void)
{
    // static bool s_done = false;      /* run only once per power-up */

    SWD_Status_t status;
    uint32_t idcode = 0U;
    uint32_t value  = 0U;
    uint32_t size_kb = 0U;
    uint32_t bad    = 0U;
    uint32_t polls  = 0U;

    static uint32_t s_runs = 0U;

    if (s_runs >= 1U) { return; }
    s_runs++;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting mass erase test");

    if (SWD_Init() != SWD_STATUS_OK)    { LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed");    return; }
    if (SWD_Connect() != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed"); return; }

    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: DPIDR failed: %s", SWD_TestStatusName(status)); return; }

    status = SWD_RequestDebugSystemPower();
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Power-up failed: %s", SWD_TestStatusName(status)); return; }

    status = SWD_Mem_Init();
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: MEM-AP init failed: %s", SWD_TestStatusName(status)); return; }

    status = SWD_Mem_Write32(CORTEX_DHCSR, DHCSR_HALT_ENABLE);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Halt failed: %s", SWD_TestStatusName(status)); return; }

    status = SWD_Mem_Read32(G0_FLASHSIZE_ADDR, &size_kb);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Flash size read failed: %s", SWD_TestStatusName(status)); return; }
    size_kb &= 0x7FUL;
    LOG_INFO(LOG_MODULE_SWD, "SWD: Flash size = %d KB", (int)size_kb);

    /* The controller must be idle and error-free before starting. */
    status = SWD_Mem_Read32(G0_FLASH_SR, &value);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_SR read failed: %s", SWD_TestStatusName(status)); return; }

    if ((value & (G0_FLASH_SR_BSY1 | G0_FLASH_SR_CFGBSY | G0_FLASH_SR_ERR_MASK)) != 0U)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_SR not idle/clean (0x%X), aborting", (unsigned long)value);
        return;
    }

    /* Unlock only if locked. */
    status = SWD_Mem_Read32(G0_FLASH_CR, &value);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_CR read failed: %s", SWD_TestStatusName(status)); return; }

    if ((value & G0_FLASH_CR_LOCK) != 0U)
    {
        status = SWD_Mem_Write32(G0_FLASH_KEYR, G0_FLASH_KEY1);
        if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: KEY1 write failed: %s", SWD_TestStatusName(status)); return; }

        status = SWD_Mem_Write32(G0_FLASH_KEYR, G0_FLASH_KEY2);
        if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: KEY2 write failed: %s", SWD_TestStatusName(status)); return; }
    }

    /* Mass erase: set MER1, then STRT. */
    status = SWD_Mem_Write32(G0_FLASH_CR, G0_FLASH_CR_MER1);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: MER1 write failed: %s", SWD_TestStatusName(status)); goto lock; }

    status = SWD_Mem_Write32(G0_FLASH_CR, G0_FLASH_CR_MER1 | G0_FLASH_CR_STRT);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: STRT write failed: %s", SWD_TestStatusName(status)); goto lock; }

    LOG_INFO(LOG_MODULE_SWD, "SWD: Mass erase started");

    /* Wait until BSY1 clears. */
    for (polls = 0U; polls < G0_ERASE_POLL_LIMIT; polls++)
    {
        status = SWD_Mem_Read32(G0_FLASH_SR, &value);
        if (status != SWD_STATUS_OK)
        {
            LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_SR poll failed: %s", SWD_TestStatusName(status));
            goto lock;
        }

        if ((value & G0_FLASH_SR_BSY1) == 0U)
        {
            break;
        }
    }

    if (polls == G0_ERASE_POLL_LIMIT)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Mass erase timeout, FLASH_SR = 0x%X", (unsigned long)value);
        goto lock;
    }

    LOG_INFO(LOG_MODULE_SWD, "SWD: Mass erase done after %d polls, FLASH_SR = 0x%X",
             (int)polls, (unsigned long)value);

    if ((value & G0_FLASH_SR_ERR_MASK) != 0U)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Mass erase reported error flags: 0x%X",
                  (unsigned long)(value & G0_FLASH_SR_ERR_MASK));
        goto lock;
    }

    /* Clear EOP (write 1) and the MER1 request. */
    (void)SWD_Mem_Write32(G0_FLASH_SR, G0_FLASH_SR_EOP);
    (void)SWD_Mem_Write32(G0_FLASH_CR, 0U);

lock:
    (void)SWD_Mem_Write32(G0_FLASH_CR, G0_FLASH_CR_LOCK);

    /* Blank check over the whole Flash. */
    if (SWD_TestCountNonBlank(G0_FLASH_START, (size_kb * 1024UL) / 4UL, &bad))
    {
        LOG_INFO(LOG_MODULE_SWD, "SWD: Blank check: %d non-blank words, %s",
                 (int)bad, (bad == 0U) ? "FLASH IS BLANK" : "NOT BLANK");
    }
    else
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Blank check read failed");
    }

    /* The core is left halted on purpose: the next step programs the Flash. */
}


#define G0_FLASH_CR_PG          (1UL << 0U)

#define G0_PROGRAM_TEST_ADDR    0x08000000UL
#define G0_PROGRAM_POLL_LIMIT   2000U

static const uint32_t s_program_pattern[8] =
{
    0x20002000UL, 0x08000101UL, 0xDEADBEEFUL, 0xCAFEBABEUL,
    0x01234567UL, 0x89ABCDEFUL, 0x0F1E2D3CUL, 0x4B5A6978UL
};


/* DPIDR, power-up, MEM-AP init, halt. Logs the failing step. */
static bool SWD_TestAttachAndHalt(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;

    if (SWD_Init() != SWD_STATUS_OK)    { LOG_ERROR(LOG_MODULE_SWD, "SWD: Init failed");    return false; }
    if (SWD_Connect() != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Connect failed"); return false; }

    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: DPIDR failed: %s", SWD_TestStatusName(status)); return false; }

    status = SWD_RequestDebugSystemPower();
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Power-up failed: %s", SWD_TestStatusName(status)); return false; }

    status = SWD_Mem_Init();
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: MEM-AP init failed: %s", SWD_TestStatusName(status)); return false; }

    status = SWD_Mem_Write32(CORTEX_DHCSR, DHCSR_HALT_ENABLE);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: Halt failed: %s", SWD_TestStatusName(status)); return false; }

    return true;
}


/* Wait for BSY1 to clear. Returns the last FLASH_SR in *sr. */
static SWD_Status_t SWD_TestFlashWaitIdle(uint32_t *sr)
{
    SWD_Status_t status;

    for (uint32_t i = 0U; i < G0_PROGRAM_POLL_LIMIT; i++)
    {
        status = SWD_Mem_Read32(G0_FLASH_SR, sr);
        if (status != SWD_STATUS_OK)
        {
            return status;
        }

        if ((*sr & G0_FLASH_SR_BSY1) == 0U)
        {
            return SWD_STATUS_OK;
        }
    }

    return SWD_STATUS_TIMEOUT;
}


/* Program one aligned 64-bit double-word. Flash must be unlocked. */
static SWD_Status_t SWD_TestFlashProgramDW(uint32_t address, uint32_t lo, uint32_t hi, uint32_t *sr)
{
    SWD_Status_t status;

    status = SWD_Mem_Write32(G0_FLASH_CR, G0_FLASH_CR_PG);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(address, lo);
    if (status != SWD_STATUS_OK) { goto done; }

    status = SWD_Mem_Write32(address + 4UL, hi);
    if (status != SWD_STATUS_OK) { goto done; }

    status = SWD_TestFlashWaitIdle(sr);
    if (status == SWD_STATUS_OK && (*sr & G0_FLASH_SR_ERR_MASK) != 0U)
    {
        status = SWD_STATUS_FAULT;      /* controller reported an error flag */
    }

done:
    (void)SWD_Mem_Write32(G0_FLASH_CR, 0U);     /* clear PG */
    return status;
}


void SWD_TestFlashProgram(void)
{
    static bool s_done = false;      /* run only once per power-up */

    SWD_Status_t status;
    uint32_t value = 0U;
    uint32_t sr    = 0U;
    uint32_t bad   = 0U;
    bool     ok    = true;

    if (s_done) { return; }
    s_done = true;

    LOG_INFO(LOG_MODULE_SWD, "SWD: Starting Flash program test");

    if (!SWD_TestAttachAndHalt()) { return; }

    /* The target area must be blank (0xFF); erase first otherwise. */
    if (!SWD_TestCountNonBlank(G0_PROGRAM_TEST_ADDR, 8U, &bad))
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Pre-check read failed");
        return;
    }
    if (bad != 0U)
    {
        LOG_ERROR(LOG_MODULE_SWD, "SWD: Target area not blank (%d words), run mass erase first", (int)bad);
        return;
    }

    status = SWD_Mem_Read32(G0_FLASH_CR, &value);
    if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: FLASH_CR read failed: %s", SWD_TestStatusName(status)); return; }

    if ((value & G0_FLASH_CR_LOCK) != 0U)
    {
        status = SWD_Mem_Write32(G0_FLASH_KEYR, G0_FLASH_KEY1);
        if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: KEY1 write failed: %s", SWD_TestStatusName(status)); return; }

        status = SWD_Mem_Write32(G0_FLASH_KEYR, G0_FLASH_KEY2);
        if (status != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "SWD: KEY2 write failed: %s", SWD_TestStatusName(status)); return; }
    }

    for (uint32_t i = 0U; i < 4U; i++)
    {
        uint32_t address = G0_PROGRAM_TEST_ADDR + (i * 8UL);

        status = SWD_TestFlashProgramDW(address,
                                        s_program_pattern[i * 2U],
                                        s_program_pattern[(i * 2U) + 1U],
                                        &sr);
        if (status != SWD_STATUS_OK)
        {
            LOG_ERROR(LOG_MODULE_SWD, "SWD: Program DW %d at 0x%X failed: %s, FLASH_SR = 0x%X",
                      (int)i, (unsigned long)address, SWD_TestStatusName(status), (unsigned long)sr);
            ok = false;
            break;
        }
    }

    (void)SWD_Mem_Write32(G0_FLASH_CR, G0_FLASH_CR_LOCK);

    if (!ok) { return; }

    /* Read back and compare. */
    bad = 0U;
    for (uint32_t i = 0U; i < 8U; i++)
    {
        status = SWD_Mem_Read32(G0_PROGRAM_TEST_ADDR + (i * 4UL), &value);
        if (status != SWD_STATUS_OK)
        {
            LOG_ERROR(LOG_MODULE_SWD, "SWD: Read-back failed: %s", SWD_TestStatusName(status));
            return;
        }

        if (value != s_program_pattern[i])
        {
            LOG_ERROR(LOG_MODULE_SWD, "SWD: Mismatch word %d: wrote 0x%X, read 0x%X",
                      (int)i, (unsigned long)s_program_pattern[i], (unsigned long)value);
            bad++;
        }
    }

    LOG_INFO(LOG_MODULE_SWD, "SWD: Program verify: %d mismatches, %s",
             (int)bad, (bad == 0U) ? "VERIFY OK" : "VERIFY FAILED");
}