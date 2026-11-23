#include <stddef.h>
#include "swd_flash.h"
#include "../mem/swd_mem.h"

#define FLASH_BASE_ADDR   0x40022000UL
#define FLASH_KEYR        (FLASH_BASE_ADDR + 0x08UL)
#define FLASH_SR          (FLASH_BASE_ADDR + 0x10UL)
#define FLASH_CR          (FLASH_BASE_ADDR + 0x14UL)

#define FLASH_CR_PG       (1UL << 0U)
#define FLASH_CR_MER1     (1UL << 2U)
#define FLASH_CR_STRT     (1UL << 16U)
#define FLASH_CR_LOCK     (1UL << 31U)

#define FLASH_SR_EOP      (1UL << 0U)
#define FLASH_SR_BSY1     (1UL << 16U)
#define FLASH_SR_CFGBSY   (1UL << 18U)
#define FLASH_SR_ERR_MASK 0x000083FAUL

#define FLASH_KEY1        0x45670123UL
#define FLASH_KEY2        0xCDEF89ABUL

#define FLASH_POLL_LIMIT  2000U


static SWD_Status_t Flash_WaitIdle(uint32_t *sr)
{
    SWD_Status_t status;

    for (uint32_t i = 0U; i < FLASH_POLL_LIMIT; i++)
    {
        status = SWD_Mem_Read32(FLASH_SR, sr);
        if (status != SWD_STATUS_OK) { return status; }

        if ((*sr & FLASH_SR_BSY1) == 0U) { return SWD_STATUS_OK; }
    }

    return SWD_STATUS_TIMEOUT;
}


SWD_Status_t SWD_Flash_Unlock(void)
{
    SWD_Status_t status;
    uint32_t cr = 0U;

    status = SWD_Mem_Read32(FLASH_CR, &cr);
    if (status != SWD_STATUS_OK) { return status; }

    if ((cr & FLASH_CR_LOCK) == 0U) { return SWD_STATUS_OK; }

    status = SWD_Mem_Write32(FLASH_KEYR, FLASH_KEY1);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(FLASH_KEYR, FLASH_KEY2);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Read32(FLASH_CR, &cr);
    if (status != SWD_STATUS_OK) { return status; }

    return ((cr & FLASH_CR_LOCK) == 0U) ? SWD_STATUS_OK : SWD_STATUS_FAULT;
}


SWD_Status_t SWD_Flash_Lock(void)
{
    return SWD_Mem_Write32(FLASH_CR, FLASH_CR_LOCK);
}


SWD_Status_t SWD_Flash_MassErase(void)
{
    SWD_Status_t status;
    uint32_t sr = 0U;

    status = SWD_Mem_Read32(FLASH_SR, &sr);
    if (status != SWD_STATUS_OK) { return status; }

    if ((sr & (FLASH_SR_BSY1 | FLASH_SR_CFGBSY | FLASH_SR_ERR_MASK)) != 0U)
    {
        return SWD_STATUS_FAULT;        /* not idle or error flags pending */
    }

    status = SWD_Mem_Write32(FLASH_CR, FLASH_CR_MER1);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(FLASH_CR, FLASH_CR_MER1 | FLASH_CR_STRT);
    if (status != SWD_STATUS_OK) { return status; }

    status = Flash_WaitIdle(&sr);

    (void)SWD_Mem_Write32(FLASH_CR, 0U);            /* clear MER1 */

    if (status != SWD_STATUS_OK) { return status; }

    if ((sr & FLASH_SR_ERR_MASK) != 0U)
    {
        (void)SWD_Mem_Write32(FLASH_SR, FLASH_SR_ERR_MASK);   /* assumed write-1-to-clear */
        return SWD_STATUS_FAULT;
    }

    (void)SWD_Mem_Write32(FLASH_SR, FLASH_SR_EOP);
    return SWD_STATUS_OK;
}


static SWD_Status_t Flash_ProgramDW(uint32_t address, uint32_t lo, uint32_t hi)
{
    SWD_Status_t status;
    uint32_t sr = 0U;

    status = SWD_Mem_Write32(FLASH_CR, FLASH_CR_PG);
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Mem_Write32(address, lo);
    if (status == SWD_STATUS_OK) { status = SWD_Mem_Write32(address + 4UL, hi); }
    if (status == SWD_STATUS_OK) { status = Flash_WaitIdle(&sr); }

    if (status == SWD_STATUS_OK && (sr & FLASH_SR_ERR_MASK) != 0U)
    {
        (void)SWD_Mem_Write32(FLASH_SR, FLASH_SR_ERR_MASK);   /* assumed write-1-to-clear */
        status = SWD_STATUS_FAULT;
    }

    (void)SWD_Mem_Write32(FLASH_CR, 0U);                      /* clear PG */
    return status;
}


SWD_Status_t SWD_Flash_Program(uint32_t address, const uint32_t *data, uint32_t words)
{
    SWD_Status_t status;

    if ((data == NULL) || ((address & 0x7UL) != 0U) || ((words & 1U) != 0U))
    {
        return SWD_STATUS_INVALID_PARAM;
    }

    for (uint32_t i = 0U; i < words; i += 2U)
    {
        status = Flash_ProgramDW(address + (i * 4UL), data[i], data[i + 1U]);
        if (status != SWD_STATUS_OK) { return status; }
    }

    return SWD_STATUS_OK;
}


SWD_Status_t SWD_Flash_Verify(uint32_t address, const uint32_t *data,
                              uint32_t words, uint32_t *mismatches)
{
    SWD_Status_t status;
    uint32_t value = 0U;

    if ((data == NULL) || (mismatches == NULL)) { return SWD_STATUS_INVALID_PARAM; }

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

    if (non_blank == NULL) { return SWD_STATUS_INVALID_PARAM; }

    *non_blank = 0U;

    for (uint32_t i = 0U; i < words; i++)
    {
        status = SWD_Mem_Read32(address + (i * 4UL), &value);
        if (status != SWD_STATUS_OK) { return status; }

        if (value != 0xFFFFFFFFUL) { (*non_blank)++; }
    }

    return SWD_STATUS_OK;
}