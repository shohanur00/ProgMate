#include "firmware.h"
#include "stm32g431xx.h"
#include "swd/physical_layer/swd_phy.h"
#include "systemclock/systemclock.h"
#include "gpio/gpio.h"
#include "board.h"
#include "timecore/timecore.h"
#include "logger/frontend/logger.h"
#include "version.h"
#include "swd/physical_layer/swd_phy.h"
#include "swd/test/swd_test.h"
#include "swd/test/swd_phy_test.h"
#include <stdbool.h>


/* --------------------------------------------------------------------------
 * Firmware Setup
 * -------------------------------------------------------------------------- */

static uint8_t LED_timer; // Define the GPIO pin for the LED

void Firmware_Setup(void)
{
    SystemClock_Init();

    GPIO_Init();

    TimeCore_Init();

    LOG_Init();

    Version_LOG();
    // SWD_Test_Run();
    
    // SWD_PHY_Test_Run();
    // SWD_PHY_Init();

    LED_timer = TimeCore_CreateTimer(200);

    TimeCore_SetDurationSecurely(LED_timer, 1000);

    TimeCore_StartTimer(LED_timer);

}

/* --------------------------------------------------------------------------
 * Firmware Loop
 * -------------------------------------------------------------------------- */

void Firmware_Loop(void)
{
    /* Firmware main loop */
    static bool state = false;

    if(TimeCore_ContinousExpiredEvent(LED_timer))
    {
        GPIO_Toggle(GPIO_LED); // Toggle the LED state
        // LOG_INFO(LOG_MODULE_SYSTEM,"Firmware Running..."); // Log a message indicating that the firmware is running
        // LOG_Disable(); 
        // if(state == false)
        // {
        //     state = true;
            
        // }
        // else
        // {
        //     state = false;
        // }
        // SWD_PHY_WriteData(state);
        // SWD_PHY_WriteClock(state);
        SWD_PHY_TestReadDPIDCODE();

    }

    TimeCore_MainLoop();                                    // Call the main loop function for time-based tasks
    LOG_MainLoop(TimeCore_GetTick());
    
}