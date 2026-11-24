#ifndef SWD_FLASH_TEST_H
#define SWD_FLASH_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Non-destructive: parameter/range checks, locked-state rejection,
 * unlock/lock. Never changes the target Flash. Safe to call repeatedly.
 */
void SWD_FlashTestSafe(void);

/*
 * DESTRUCTIVE: mass-erases the target Flash, then programs and verifies
 * test data. Runs at most SWD_FLASH_TEST_MAX_RUNS times per power-up.
 * Leaves the target core halted (its Flash no longer holds valid firmware).
 */
void SWD_FlashTestAll(void);

/*
* DESTRUCTIVE: replaces the whole target Flash with g_target_image. 
* Leaves the target core halted (its Flash no longer holds valid firmware).
*/

void SWD_FlashTestImage(void);

#ifdef __cplusplus
}
#endif

#endif /* SWD_FLASH_TEST_H */