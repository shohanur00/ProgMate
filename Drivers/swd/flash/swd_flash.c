#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "swd_flash.h"
#include "../mem/swd_mem.h"

/*
 * STM32G0 Flash controller over MEM-AP.
 *
 * Register offsets and bit positions: checked against ST's cmsis_device_g0.
 * Base address and unlock keys: confirmed on hardware (STM32G030F6).
 * The G0F_ prefix avoids clashing with the FLASH_* macros of the
 * programmer's own CMSIS header (STM32G431).
 */

/* --------------------------------------------------------------------------
 * Target constants
 * -------------------------------------------------------------------------- */

#define G0F_FLASH_START     0x08000000UL

/* Temporary: STM32G030F6 (32 KB). Later this should come from the target layer
 * (Flash size register at 0x1FFF75E0). */
#define G0F_FLASH_MAX       (32UL * 1024UL)

#define G0F_REG_BASE        0x40022000UL
#define G0F_REG_KEYR        (G0F_REG_BASE + 0x08UL)
#define G0F_REG_SR          (G0F_REG_BASE + 0x10UL)
#define G0F_REG_CR          (G0F_REG_BASE + 0x14UL)

#define G0F_KEY1            0x45670123UL
#define G0F_KEY2            0xCDEF89ABUL

#define G0F_CR_PG           (1UL << 0U)
#define G0F_CR_MER1         (1UL << 2U)
#define G0F_CR_STRT         (1UL << 16U)
#define G0F_CR_LOCK         (1UL << 31U)

#define G0F_SR_EOP          (1UL << 0U)
#define G0F_SR_BSY1         (1UL << 16U)
#define G0F_SR_CFGBSY       (1UL << 18U)
/* OPERR, PROGERR, WRPERR, PGAERR, SIZERR, PGSERR, MISERR, FASTERR, OPTVERR */
#define G0F_SR_ERR_MASK     0x000083FAUL

#define G0F_POLL_LIMIT      2000U


/* --------------------------------------------------------------------------
 * Private helpers
 * -------------------------------------------------------------------------- */

/* Word-aligned range that lies completely inside the Flash. */
static bool Flash_RangeValid(uint32_t address, uint32_t words)
{
    uint32_t offset;

    if ((address & 3UL) != 0U)          { return false; }
    if (address < G0F_FLASH_START)      { return false; }

    offset = address - G0F_FLASH_START;
    if (offset > G0F_FLASH_MAX)         { return false; }

    return (words <= ((G0F_FLASH_MAX - offset) / 4UL));
}


/* Wait until BSY1 clears. The last FLASH_SR value is returned in *sr. */
static SWD_Status_t Flash_WaitIdle(uint32_t *sr)
{
    SWD_Status_t status;

    for (uint32_t i = 0U; i < G0F_POLL_LIMIT; i++)
    {
        status = SWD_Mem_Read32(G0F_REG_SR, sr);
        if (status != SWD_STATUS_OK) { return status; }

        if ((*sr & G0F_SR_BSY1) == 0U) { return SWD_STATUS_OK; }
    }

    return SWD_STATUS_TIMEOUT;
}


/*
 * Check the controller before an erase or program operation:
 *   - Flash must be unlocked           -> FAULT otherwise
 *   - controller must not be busy      -> WAIT otherwise
 *   - stale error/EOP flags are cleared (write 1 to clear, assumed) and
 *     re-read; flags that survive the clear -> FAULT
 */
static SWD_Status_t Flash_Prepare(void)
{
    SWD_Status_t status;
    uint32_t cr = 0U;
    uint32_t sr = 0U;

    status = SWD_Mem_Read32(G0F_REG_CR, &cr);
    if (status != SWD_STATUS_OK) { return status; }

    if ((cr & G0F_CR_LOCK) != 0U) { return SWD_STATUS_FAULT; }

    status = SWD_Mem_Read32(G0F_REG_SR, &sr);
    if (status != SWD_STATUS_OK) { return status; }

    if ((sr & (G0F_SR_BSY1 | G0F_SR_CFGBSY)) != 0U) { return SWD_STATUS_WAIT; }

    if ((sr & (G0F_SR_ERR_MASK | G0F_SR_EOP)) != 0U)
    {
        status = SWD_Mem_Write32(G0F_REG_SR, sr & (G0F_SR_ERR_MASK | G0F_SR_EOP));
        if (status != SWD_STATUS_OK) { return status; }

        status = SWD_Mem_Read32(G0F_REG_SR, &sr);
        if (status != SWD_STATUS_OK) { return status; }

        if ((sr & G0F_SR_ERR_MASK) != 0U) { return SWD_STATUS_FAULT; }
    }

    return SWD_STATUS_OK;
}


/* --------------------------------------------------------------------------
 * Lock / unlock
 * -------------------------------------------------------------------------- */

SWD_Status_t SWD_Flash_Unlock(void)
{
    SWD_Status_t status;
    uint32_t cr = 0U;

    status = SWD_Mem_Read32(G0F_REG_CR, &cr);
    if (status != SWD_STATUS_OK) { return status; }

    if ((cr & G0F_CR_LOCK) == 0U) { return SWD_STATUS_OK; }     /* already unlocked */

    /* Exactly two keys: a wrong sequence keeps the controller locked until reset. */
    status = SWD_Mem_Write32(G0F_REG_KEYR, G0F_KEY1);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(G0F_REG_KEYR, G0F_KEY2);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Read32(G0F_REG_CR, &cr);
    if (status != SWD_STATUS_OK) { return status; }

    return ((cr & G0F_CR_LOCK) == 0U) ? SWD_STATUS_OK : SWD_STATUS_HW_ERROR;
}


SWD_Status_t SWD_Flash_Lock(void)
{
    return SWD_Mem_Write32(G0F_REG_CR, G0F_CR_LOCK);
}


/* --------------------------------------------------------------------------
 * Erase
 * -------------------------------------------------------------------------- */

SWD_Status_t SWD_Flash_MassErase(void)
{
    SWD_Status_t status;
    uint32_t sr = 0U;

    status = Flash_Prepare();
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(G0F_REG_CR, G0F_CR_MER1);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(G0F_REG_CR, G0F_CR_MER1 | G0F_CR_STRT);
    if (status == SWD_STATUS_OK)
    {
        status = Flash_WaitIdle(&sr);
    }

    (void)SWD_Mem_Write32(G0F_REG_CR, 0U);              /* clear MER1 */

    if (status != SWD_STATUS_OK) { return status; }

    if ((sr & G0F_SR_ERR_MASK) != 0U)
    {
        (void)SWD_Mem_Write32(G0F_REG_SR, sr & G0F_SR_ERR_MASK);
        return SWD_STATUS_FAULT;
    }

    (void)SWD_Mem_Write32(G0F_REG_SR, G0F_SR_EOP);
    return SWD_STATUS_OK;
}


/* --------------------------------------------------------------------------
 * Program
 * -------------------------------------------------------------------------- */

/* One aligned 64-bit double-word. The two words are written back to back. */
static SWD_Status_t Flash_ProgramDW(uint32_t address, uint32_t lo, uint32_t hi)
{
    SWD_Status_t status;
    uint32_t sr = 0U;

    status = SWD_Mem_Write32(G0F_REG_CR, G0F_CR_PG);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(address, lo);
    if (status == SWD_STATUS_OK) { status = SWD_Mem_Write32(address + 4UL, hi); }
    if (status == SWD_STATUS_OK) { status = Flash_WaitIdle(&sr); }

    if ((status == SWD_STATUS_OK) && ((sr & G0F_SR_ERR_MASK) != 0U))
    {
        (void)SWD_Mem_Write32(G0F_REG_SR, sr & G0F_SR_ERR_MASK);
        status = SWD_STATUS_FAULT;
    }

    (void)SWD_Mem_Write32(G0F_REG_CR, 0U);              /* clear PG, also after an error */
    return status;
}


SWD_Status_t SWD_Flash_Program(uint32_t address, const uint32_t *data, uint32_t words)
{
    SWD_Status_t status;

    if ((data == NULL) || ((address & 7UL) != 0U) || ((words & 1U) != 0U) ||
        !Flash_RangeValid(address, words))
    {
        return SWD_STATUS_INVALID_PARAM;
    }

    status = Flash_Prepare();
    if (status != SWD_STATUS_OK) { return status; }

    for (uint32_t i = 0U; i < words; i += 2U)
    {
        status = Flash_ProgramDW(address + (i * 4UL), data[i], data[i + 1U]);
        if (status != SWD_STATUS_OK) { return status; }
    }

    return SWD_STATUS_OK;
}


/* --------------------------------------------------------------------------
 * Verify / blank check
 * -------------------------------------------------------------------------- */

SWD_Status_t SWD_Flash_Verify(uint32_t address, const uint32_t *data,
                              uint32_t words, uint32_t *mismatches)
{
    SWD_Status_t status;
    uint32_t value = 0U;

    if ((data == NULL) || (mismatches == NULL) || !Flash_RangeValid(address, words))
    {
        return SWD_STATUS_INVALID_PARAM;
    }

    *mismatches = 0U;

    for (uint32_t i = 0U; i < words; i++)
    {
        status = SWD_Mem_Read32(address + (i * 4UL), &value);
        if (status != SWD_STATUS_OK) { return status; }

        if (value != data[i]) { (*mismatches)++; }
    }

    return SWD_STATUS_OK;
}


SWD_Status_t SWD_Flash_BlankCheck(uint32_t address, uint32_t words, uint32_t *non_blank)
{
    SWD_Status_t status;
    uint32_t value = 0U;

    if ((non_blank == NULL) || !Flash_RangeValid(address, words))
    {
        return SWD_STATUS_INVALID_PARAM;
    }

    *non_blank = 0U;

    for (uint32_t i = 0U; i < words; i++)
    {
        status = SWD_Mem_Read32(address + (i * 4UL), &value);
        if (status != SWD_STATUS_OK) { return status; }

        if (value != 0xFFFFFFFFUL) { (*non_blank)++; }
    }

    return SWD_STATUS_OK;
}