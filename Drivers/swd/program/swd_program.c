#include <stddef.h>

#include "swd_program.h"
#include "../flash/swd_flash.h"
#include "../mem/swd_mem.h"

#define PROG_FLASH_START          0x08000000UL

#define PROG_DHCSR                0xE000EDF0UL
#define PROG_DHCSR_RELEASE        0xA05F0000UL   /* DBGKEY, C_HALT and C_DEBUGEN cleared */

#define PROG_AIRCR                0xE000ED0CUL
#define PROG_AIRCR_SYSRESETREQ    0x05FA0004UL   /* VECTKEY | SYSRESETREQ */


SWD_Status_t SWD_Program_Image(const uint32_t *image, uint32_t words, uint32_t *mismatches)
{
    SWD_Status_t status;

    if ((image == NULL) || (mismatches == NULL) || (words == 0U) || ((words & 1U) != 0U))
    {
        return SWD_STATUS_INVALID_PARAM;
    }

    *mismatches = 0xFFFFFFFFUL;         /* "not verified" until Verify has run */

    status = SWD_Flash_Unlock();
    if (status != SWD_STATUS_OK) { return status; }

    status = SWD_Flash_MassErase();
    if (status != SWD_STATUS_OK)
    {
        (void)SWD_Flash_Lock();
        return status;
    }

    status = SWD_Flash_Program(PROG_FLASH_START, image, words);

    (void)SWD_Flash_Lock();

    if (status != SWD_STATUS_OK) { return status; }

    return SWD_Flash_Verify(PROG_FLASH_START, image, words, mismatches);
}


SWD_Status_t SWD_Program_RunTarget(void)
{
    SWD_Status_t status;

    status = SWD_Mem_Write32(PROG_DHCSR, PROG_DHCSR_RELEASE);
    if (status != SWD_STATUS_OK) { return status; }

    /* The target resets right after this write, so the answer may be lost. */
    (void)SWD_Mem_Write32(PROG_AIRCR, PROG_AIRCR_SYSRESETREQ);

    return SWD_STATUS_OK;
}