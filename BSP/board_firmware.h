#ifndef BOARD_FIRMWARE_H
#define BOARD_FIRMWARE_H

#include "stm32g431xx.h"
#include "cdefs/cdefs.h"



/* ============================================================================
 * Logger UART Configuration
 * ============================================================================
 *
 * LOG_UART_INSTANCE selects the UART peripheral used by the Logger.
 *
 * Supported values:
 *   1U - USART1
 *   2U - USART2
 *   3U - USART3
 *   4U - UART4
 *   5U - UART5
 *   6U - LPUART1
 *   0U - DISABLE
 * ========================================================================== */

#define LOG_USE_UART                 ENABLE
#define LOG_USE_FLASH                DISABLE
#define LOG_USE_USB                  DISABLE
#define LOG_USE_RTT                  DISABLE

#define LOG_UART_INSTANCE            3U
#define LOG_UART_BAUDRATE            115200U
#define LOG_UART_PARITY              UART_PARITY_NONE
#define LOG_UART_STOP_BITS           UART_STOPBITS_1


#if LOG_USE_UART == ENABLE

#define LOG_UART_TX_PORT             GPIOB
#define LOG_UART_TX_PIN              9U

#else

#define LOG_UART_TX_PORT             NULL
#define LOG_UART_TX_PIN              0

#endif


/* ============================================================================
 * SWD Programmer Configuration
 * ========================================================================== */

/* SWD clock output */
#define SWD_SWCLK_PORT                      GPIOC
#define SWD_SWCLK_PIN                       13U

/* SWD bidirectional data */
#define SWD_SWDIO_PORT                      GPIOC
#define SWD_SWDIO_PIN                       14U

#define SWD_LINE_RESET_CYCLES               56U
#define SWD_TURNAROUND_CYCLES               1U




/* ============================================================================
 * GPIO Configuration
 * ========================================================================== */

#define LED_PORT                            GPIOA
#define LED_PIN                             0U


#endif /* BOARD_FIRMWARE_H */