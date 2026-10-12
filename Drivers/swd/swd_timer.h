/**

* @file    swd_timer.h
* @brief   TIM6-based timer service for SWD operations.
*
* @details Provides periodic timer interrupts for driving non-blocking
* ```
       SWD state-machine operations.
  ```
*
* ```
       The TIM6 interrupt handler must call SWD_Timer_IRQHandler().
  ```
*
* @note    The callback executes in interrupt context and must not block.
  */

#ifndef SWD_TIMER_H
#define SWD_TIMER_H

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------

* Callback type
* -------------------------------------------------------------------------- */

typedef void (*SWD_TimerCallback_t)(void);

/* --------------------------------------------------------------------------

* Public API
* -------------------------------------------------------------------------- */

/**

* @brief Initialize the SWD timer.
*
* @param tick_hz Requested timer interrupt frequency in Hz.
* @return true if initialization succeeds; otherwise false.
  */
  bool SWD_Timer_Init(uint32_t tick_hz);

/**

* @brief Start periodic timer interrupts.
*
* @return true if the timer starts successfully; otherwise false.
  */
  bool SWD_Timer_Start(void);

/**

* @brief Stop the timer and its update interrupts.
  */
  void SWD_Timer_Stop(void);

/**

* @brief Check whether the timer is running.
*
* @return true if running; otherwise false.
  */
  bool SWD_Timer_IsRunning(void);

/**

* @brief Register a callback for each timer tick.
*
* @param callback Callback function, or NULL to disable the callback.
  */
  void SWD_Timer_SetCallback(SWD_TimerCallback_t callback);

/**

* @brief Process the TIM6 update interrupt.
*
* @note Call from the actual TIM6 IRQ handler after confirming that
* ```
    the TIM6 update interrupt is pending.
  ```

*/
void SWD_Timer_IRQHandler(void);

#endif /* SWD_TIMER_H */
