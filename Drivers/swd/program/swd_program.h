#ifndef SWD_PROGRAM_H
#define SWD_PROGRAM_H

#include <stdint.h>
#include "../swd_types.h"

/*
 * Program a complete image at the start of the target Flash.
 * Precondition: DPIDR read, power-up, SWD_Mem_Init() done, core halted.
 *
 * Sequence: unlock, mass erase, program, lock, verify.
 * The return value only says that the operations ran. The caller must also
 * check *mismatches == 0 for a successful verify.
 *
 *   words must be even (double-word aligned image).
 */
SWD_Status_t SWD_Program_Image(const uint32_t *image, uint32_t words, uint32_t *mismatches);

/* Release the halted core and reset the target so it runs the new image. */
SWD_Status_t SWD_Program_RunTarget(void);

#endif /* SWD_PROGRAM_H */