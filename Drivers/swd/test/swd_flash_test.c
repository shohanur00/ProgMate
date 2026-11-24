#include <stdint.h>
#include <stdbool.h>

#include "swd_flash_test.h"

#include "../swd.h"
#include "../mem/swd_mem.h"
#include "../flash/swd_flash.h"
#include "../program/swd_program.h"
#include "target_image.h"

#include "logger/frontend/logger.h"
#include "timecore/timecore.h"


/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

#ifndef SWD_FLASH_TEST_MAX_RUNS
#define SWD_FLASH_TEST_MAX_RUNS     5U
#endif

/* STM32G030F6 (temporary, matches G0F_FLASH_MAX in swd_flash.c). */
#define TEST_FLASH_START            0x08000000UL
#define TEST_FLASH_WORDS            8192UL          /* 32 KB */

#define TEST_BLOCK_ADDRESS          0x08000400UL
#define TEST_BLOCK_WORDS            256U            /* 1 KB */

#define TEST_CORTEX_DHCSR           0xE000EDF0UL
#define TEST_DHCSR_HALT             0xA05F0003UL    /* DBGKEY | C_HALT | C_DEBUGEN */
#define TEST_DHCSR_RESUME           0xA05F0000UL    /* DBGKEY */

#define TEST_FLASH_CR_ADDR          0x40022014UL
#define TEST_FLASH_CR_LOCK          (1UL << 31U)


/* --------------------------------------------------------------------------
 * State and helpers
 * -------------------------------------------------------------------------- */

static uint32_t s_pass;
static uint32_t s_fail;
static bool     s_erased;       /* mass erase + blank check succeeded in this run */

static const uint32_t s_pattern[8] =
{
    0x20002000UL, 0x08000101UL, 0xDEADBEEFUL, 0xCAFEBABEUL,
    0x01234567UL, 0x89ABCDEFUL, 0x0F1E2D3CUL, 0x4B5A6978UL
};

static uint32_t s_block[TEST_BLOCK_WORDS];


static const char *StatusName(SWD_Status_t status)
{
    switch (status)
    {
        case SWD_STATUS_OK:            return "OK";
        case SWD_STATUS_WAIT:          return "WAIT";
        case SWD_STATUS_FAULT:         return "FAULT";
        case SWD_STATUS_NO_ACK:        return "NO_ACK";
        case SWD_STATUS_PARITY:        return "PARITY";
        case SWD_STATUS_TIMEOUT:       return "TIMEOUT";
        case SWD_STATUS_PARAM:         return "PARAM";
        case SWD_STATUS_HW_ERROR:      return "HW_ERROR";
        case SWD_STATUS_INVALID_PARAM: return "INVALID_PARAM";
        default:                       return "UNKNOWN";
    }
}


static void Check(const char *name, bool ok)
{
    if (ok)
    {
        s_pass++;
        LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: %s: PASS", name);
    }
    else
    {
        s_fail++;
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: %s: FAIL", name);
    }
}


/* Check a status code against the expected one. Returns true on match. */
static bool Expect(const char *name, SWD_Status_t got, SWD_Status_t want)
{
    bool ok = (got == want);

    if (!ok)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: %s: got %s, expected %s",
                  name, StatusName(got), StatusName(want));
    }

    Check(name, ok);
    return ok;
}


static uint32_t Ticks(void)
{
    return (uint32_t)TimeCore_GetTick();
}


static void ResetCounters(void)
{
    s_pass   = 0U;
    s_fail   = 0U;
    s_erased = false;
}


static void Summary(void)
{
    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: summary: %d passed, %d failed, %s",
             (int)s_pass, (int)s_fail, (s_fail == 0U) ? "ALL OK" : "FAILURES");
}


/* DPIDR, power-up, MEM-AP init, halt. Logs the failing step. */
static bool Attach(void)
{
    SWD_Status_t status;
    uint32_t idcode = 0U;

    if (SWD_Init() != SWD_STATUS_OK)    { LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: Init failed");    return false; }
    if (SWD_Connect() != SWD_STATUS_OK) { LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: Connect failed"); return false; }

    status = SWD_ReadDPIDCODE(&idcode);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: DPIDR failed: %s", StatusName(status));
        return false;
    }

    status = SWD_RequestDebugSystemPower();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: Power-up failed: %s", StatusName(status));
        return false;
    }

    status = SWD_Mem_Init();
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: MEM-AP init failed: %s", StatusName(status));
        return false;
    }

    status = SWD_Mem_Write32(TEST_CORTEX_DHCSR, TEST_DHCSR_HALT);
    if (status != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: Halt failed: %s", StatusName(status));
        return false;
    }

    return true;
}


/* Reads FLASH_CR and reports the LOCK bit. Returns false on a read error. */
static bool ReadLocked(bool *locked)
{
    uint32_t cr = 0U;

    if (SWD_Mem_Read32(TEST_FLASH_CR_ADDR, &cr) != SWD_STATUS_OK)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: FLASH_CR read failed");
        return false;
    }

    *locked = ((cr & TEST_FLASH_CR_LOCK) != 0U);
    return true;
}


/* --------------------------------------------------------------------------
 * Tests
 * -------------------------------------------------------------------------- */

/* Parameter and range checks. Nothing here writes to the target Flash. */
static void Test_Params(void)
{
    uint32_t count = 0U;

    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: --- parameter checks ---");

    (void)SWD_Flash_Lock();     /* so a rejected request cannot touch the Flash */

    (void)Expect("program: NULL data",
                 SWD_Flash_Program(TEST_FLASH_START, NULL, 2U), SWD_STATUS_INVALID_PARAM);

    (void)Expect("program: unaligned (word)",
                 SWD_Flash_Program(TEST_FLASH_START + 1UL, s_pattern, 2U), SWD_STATUS_INVALID_PARAM);

    (void)Expect("program: not 8-byte aligned",
                 SWD_Flash_Program(TEST_FLASH_START + 4UL, s_pattern, 2U), SWD_STATUS_INVALID_PARAM);

    (void)Expect("program: odd word count",
                 SWD_Flash_Program(TEST_FLASH_START, s_pattern, 3U), SWD_STATUS_INVALID_PARAM);

    (void)Expect("program: address in SRAM",
                 SWD_Flash_Program(0x20000000UL, s_pattern, 2U), SWD_STATUS_INVALID_PARAM);

    (void)Expect("program: just past the end of Flash",
                 SWD_Flash_Program(TEST_FLASH_START + (TEST_FLASH_WORDS * 4UL), s_pattern, 2U),
                 SWD_STATUS_INVALID_PARAM);

    (void)Expect("verify: too many words",
                 SWD_Flash_Verify(TEST_FLASH_START, s_pattern, (uint32_t)TEST_FLASH_WORDS + 1U, &count),
                 SWD_STATUS_INVALID_PARAM);

    (void)Expect("verify: NULL mismatches",
                 SWD_Flash_Verify(TEST_FLASH_START, s_pattern, 8U, NULL), SWD_STATUS_INVALID_PARAM);

    (void)Expect("blank check: below Flash",
                 SWD_Flash_BlankCheck(TEST_FLASH_START - 4UL, 1U, &count), SWD_STATUS_INVALID_PARAM);

    /* Last valid word is readable. */
    (void)Expect("blank check: last valid word",
                 SWD_Flash_BlankCheck(TEST_FLASH_START + ((TEST_FLASH_WORDS - 1UL) * 4UL), 1U, &count),
                 SWD_STATUS_OK);

    /* Valid request while locked: must be refused before any write. */
    (void)Expect("program: refused while locked",
                 SWD_Flash_Program(TEST_FLASH_START, s_pattern, 8U), SWD_STATUS_FAULT);
}


static void Test_UnlockLock(void)
{
    bool locked = false;

    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: --- unlock / lock ---");

    (void)Expect("lock", SWD_Flash_Lock(), SWD_STATUS_OK);

    if (ReadLocked(&locked)) { Check("CR.LOCK set after lock", locked); }

    if (!Expect("unlock", SWD_Flash_Unlock(), SWD_STATUS_OK))
    {
        return;
    }

    if (ReadLocked(&locked)) { Check("CR.LOCK clear after unlock", !locked); }

    (void)Expect("unlock again (already unlocked)", SWD_Flash_Unlock(), SWD_STATUS_OK);

    (void)Expect("lock again", SWD_Flash_Lock(), SWD_STATUS_OK);

    if (ReadLocked(&locked)) { Check("CR.LOCK set after second lock", locked); }
}


/* DESTRUCTIVE: erases the whole Flash. */
static void Test_Erase(void)
{
    uint32_t non_blank = 0xFFFFFFFFUL;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;

    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: --- mass erase ---");

    (void)SWD_Flash_Lock();
    (void)Expect("mass erase refused while locked", SWD_Flash_MassErase(), SWD_STATUS_FAULT);

    if (!Expect("unlock", SWD_Flash_Unlock(), SWD_STATUS_OK)) { return; }

    t0 = Ticks();
    if (!Expect("mass erase", SWD_Flash_MassErase(), SWD_STATUS_OK))
    {
        (void)SWD_Flash_Lock();
        return;
    }
    t1 = Ticks();

    (void)SWD_Flash_Lock();

    if (Expect("blank check (full Flash)",
               SWD_Flash_BlankCheck(TEST_FLASH_START, (uint32_t)TEST_FLASH_WORDS, &non_blank),
               SWD_STATUS_OK))
    {
        t2 = Ticks();
        LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: non-blank words = %d", (int)non_blank);
        Check("Flash is blank after erase", non_blank == 0U);
        LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: erase %d ticks, blank check %d ticks",
                 (int)(t1 - t0), (int)(t2 - t1));

        s_erased = (non_blank == 0U);
    }
}


/* DESTRUCTIVE: writes 32 bytes at the start of Flash. */
static void Test_Program32(void)
{
    uint32_t mismatches = 0xFFFFFFFFUL;
    uint32_t corrupted[8];

    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: --- program 32 bytes ---");

    if (!s_erased)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: skipped, Flash is not known to be blank");
        return;
    }

    if (!Expect("unlock", SWD_Flash_Unlock(), SWD_STATUS_OK)) { return; }

    (void)Expect("program 8 words at start of Flash",
                 SWD_Flash_Program(TEST_FLASH_START, s_pattern, 8U), SWD_STATUS_OK);

    (void)SWD_Flash_Lock();

    if (Expect("verify against the written data",
               SWD_Flash_Verify(TEST_FLASH_START, s_pattern, 8U, &mismatches), SWD_STATUS_OK))
    {
        LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: mismatches = %d", (int)mismatches);
        Check("0 mismatches", mismatches == 0U);
    }

    /* The verify function itself must detect a difference. */
    for (uint32_t i = 0U; i < 8U; i++) { corrupted[i] = s_pattern[i]; }
    corrupted[3] ^= 1UL;

    if (Expect("verify against corrupted copy",
               SWD_Flash_Verify(TEST_FLASH_START, corrupted, 8U, &mismatches), SWD_STATUS_OK))
    {
        Check("exactly 1 mismatch detected", mismatches == 1U);
    }
}


/* DESTRUCTIVE: writes 1 KB at TEST_BLOCK_ADDRESS and logs the timing. */
static void Test_ProgramBlock(void)
{
    uint32_t mismatches = 0xFFFFFFFFUL;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;

    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: --- program 1 KB ---");

    if (!s_erased)
    {
        LOG_ERROR(LOG_MODULE_SWD, "FLASH TEST: skipped, Flash is not known to be blank");
        return;
    }

    for (uint32_t i = 0U; i < TEST_BLOCK_WORDS; i++)
    {
        s_block[i] = 0xA5000000UL ^ (i * 0x01010101UL);
    }

    if (!Expect("unlock", SWD_Flash_Unlock(), SWD_STATUS_OK)) { return; }

    t0 = Ticks();
    (void)Expect("program 256 words", SWD_Flash_Program(TEST_BLOCK_ADDRESS, s_block, TEST_BLOCK_WORDS), SWD_STATUS_OK);
    t1 = Ticks();

    (void)SWD_Flash_Lock();

    if (Expect("verify 256 words",
               SWD_Flash_Verify(TEST_BLOCK_ADDRESS, s_block, TEST_BLOCK_WORDS, &mismatches), SWD_STATUS_OK))
    {
        t2 = Ticks();
        LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: mismatches = %d", (int)mismatches);
        Check("0 mismatches", mismatches == 0U);
        LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: program %d ticks, verify %d ticks (1 KB)",
                 (int)(t1 - t0), (int)(t2 - t1));
    }
}


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void SWD_FlashTestSafe(void)
{
    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: starting safe tests (no Flash changes)");

    ResetCounters();

    if (!Attach()) { return; }

    Test_Params();
    Test_UnlockLock();
    Summary();

    /* Flash was not touched, so the target firmware can run again. */
    (void)SWD_Mem_Write32(TEST_CORTEX_DHCSR, TEST_DHCSR_RESUME);
}


void SWD_FlashTestAll(void)
{
    static uint32_t s_runs = 0U;

    if (s_runs >= SWD_FLASH_TEST_MAX_RUNS) { return; }
    s_runs++;

    LOG_INFO(LOG_MODULE_SWD, "FLASH TEST: starting full test %d/%d (DESTRUCTIVE)",
             (int)s_runs, (int)SWD_FLASH_TEST_MAX_RUNS);

    ResetCounters();

    if (!Attach()) { return; }

    Test_Params();
    Test_UnlockLock();
    Test_Erase();
    Test_Program32();
    Test_ProgramBlock();
    Summary();

    /* The core stays halted: the target Flash no longer holds valid firmware. */
}


/* DESTRUCTIVE: replaces the whole target Flash with g_target_image. */
void SWD_FlashTestImage(void)
{
    static uint32_t s_runs = 0U;

    SWD_Status_t status;
    uint32_t mismatches = 0xFFFFFFFFUL;
    uint32_t t0;
    uint32_t t1;

    if (s_runs >= SWD_FLASH_TEST_MAX_RUNS) { return; }
    s_runs++;

    LOG_INFO(LOG_MODULE_SWD, "IMAGE: programming %d bytes (%d words), DESTRUCTIVE",
             (int)G_TARGET_IMAGE_BYTES, (int)G_TARGET_IMAGE_WORDS);

    ResetCounters();

    if (!Attach()) { return; }

    t0 = Ticks();
    status = SWD_Program_Image(g_target_image, G_TARGET_IMAGE_WORDS, &mismatches);
    t1 = Ticks();

    if (!Expect("program image (erase, program, verify)", status, SWD_STATUS_OK)) { return; }

    LOG_INFO(LOG_MODULE_SWD, "IMAGE: mismatches = %d, took %d ticks",
             (int)mismatches, (int)(t1 - t0));

    Check("image verify: 0 mismatches", mismatches == 0U);
    if (mismatches != 0U) { return; }

    (void)Expect("release and reset target", SWD_Program_RunTarget(), SWD_STATUS_OK);

    Summary();
}