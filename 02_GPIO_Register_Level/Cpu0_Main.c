#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Ifx_Cfg_Ssw.h"
#include "IfxPort_reg.h"   // To access the Port Registers Directly
#define STM_20MS_TICKS 2000000u

IFX_ALIGN(4) IfxCpu_syncEvent cpuSyncEvent = 0;

void core0_main(void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);
    
    /**************** GPIO CONFIGURATION ****************/

    /* LED1: P00.5 -> GPIO push-pull output
     *
     * P00.5 belongs to IOCR4
     * P00.5 slot = bits 15:8
     * PC5 = 10000b -> push-pull general-purpose output
     * Complete 8-bit slot = 1000 0000b = 0x80
     * Slot starts at bit 8
     */
    MODULE_P00.IOCR4.U =
            (MODULE_P00.IOCR4.U & ~(0xFFu << 8)) |
            (0x80u << 8);


    /* LED2: P00.6 -> GPIO push-pull output
     *
     * P00.6 belongs to IOCR4
     * P00.6 slot = bits 23:16
     * PC6 = 10000b
     * Complete slot = 0x80
     * Slot starts at bit 16
     */
    MODULE_P00.IOCR4.U =
            (MODULE_P00.IOCR4.U & ~(0xFFu << 16)) |
            (0x80u << 16);


    /* BUTTON1: P00.7 -> GPIO input, no internal pull
     *
     * P00.7 belongs to IOCR4
     * P00.7 slot = bits 31:24
     * PC7 = 00000b
     * Complete slot = 0x00
     * Slot starts at bit 24
     */
    MODULE_P00.IOCR4.U =
            (MODULE_P00.IOCR4.U & ~(0xFFu << 24));


    /**************** INITIAL LED STATE ****************/

    /*
     * LEDs are active-low:
     *
     * HIGH -> LED OFF
     * LOW  -> LED ON
     *
     * OMR PS5 -> set P00.5 HIGH
     * OMR PS6 -> set P00.6 HIGH
     */

    MODULE_P00.OMR.U = (1u << 5);
    MODULE_P00.OMR.U = (1u << 6);


    /**************** VARIABLES ****************/

    /*
     * TRUE  -> button released
     * FALSE -> button pressed
     */
    boolean previousButtonState = TRUE;

    /*
     * state:
     *
     * 0 = both LEDs OFF
     * 1 = LED1 ON, LED2 OFF
     * 2 = LED1 OFF, LED2 ON
     * 3 = both LEDs ON
     */
    uint8 state = 0;
    

        /**************** MAIN LOOP ****************/

    while (1)
    {
        /* Read BUTTON1 on P00.7 directly from the IN register */
        boolean currentButtonState =
                ((MODULE_P00.IN.U & (1u << 7)) != 0u);


        /* Detect falling edge:
         *
         * previous = TRUE  -> released
         * current  = FALSE -> pressed
         */
        if ((previousButtonState == TRUE) &&    // Both conditions must happen together:
            (currentButtonState == FALSE))
        {

           
                        /**************** BUTTON DEBOUNCE ****************/

            /*
             * Save the current STM0 timer value.
             * TIM0 contains the lower 32 bits of the free-running STM counter.
             */
            uint32 startTick = MODULE_STM0.TIM0.U;


            /* Working of STM :- As our program repeatedly checks: currentTick-startTick, until enough ticks have passed to equal 20 ms.
             * Wait approximately 20 ms.
             *
             * STM_20MS_TICKS will contain the number of STM ticks
             * corresponding to 20 ms.
             * TIM0 is one of the STM timer registers.
             * We will calculate its exact value after checking fSTM.
             *//* ticks = fSTM x 0.020 seconds */

            while ((uint32)(MODULE_STM0.TIM0.U - startTick)
                    < STM_20MS_TICKS)
            {
                /* Wait */
            }


            /**************** READ BUTTON AGAIN ****************/

            /*
             * Read P00.7 again after the debounce time.
             */
            currentButtonState =
                    ((MODULE_P00.IN.U & (1u << 7)) != 0u);


            /* Confirm that the button is still pressed */
            if (currentButtonState == FALSE)
            {

                                /**************** MOVE TO NEXT STATE ****************/

                state++;

                /* After state 3, return to state 0 */
                if (state > 3)
                {
                    state = 0;
                }


                /**************** Defined LED STATE MACHINE ****************/

                switch (state)
                {
                    case 0:
                        /* Both LEDs OFF
                         *
                         * P00.5 HIGH -> LED1 OFF
                         * P00.6 HIGH -> LED2 OFF
                         */
                        MODULE_P00.OMR.U = (1u << 5);
                        MODULE_P00.OMR.U = (1u << 6);

                        break;


                    case 1:
                        /* LED1 ON, LED2 OFF
                         *
                         * P00.5 LOW  -> PCL5 = bit 21  *(5+16)* P-CLEAR5
                         * P00.6 HIGH -> PS6  = bit 6  (SET PS6)
                         */
                        MODULE_P00.OMR.U = (1u << 21);
                        MODULE_P00.OMR.U = (1u << 6);

                        break;


                    case 2:
                        /* LED1 OFF, LED2 ON
                         *
                         * P00.5 HIGH -> PS5  = bit 5 (SET)
                         * P00.6 LOW  -> PCL6 = bit 22 *(6+16)* CLEAR
                         */
                        MODULE_P00.OMR.U = (1u << 5);
                        MODULE_P00.OMR.U = (1u << 22); 

                        break;


                    case 3:
                        /* Both LEDs ON
                         * As TC375 LEDs are Active-Low
                         * P00.5 LOW -> PCL5 = bit 21   *(16+5)* P-Clear
                         * P00.6 LOW -> PCL6 = bit 22   *(16+6)* p-Clear
                         */
                        MODULE_P00.OMR.U = (1u << 21);
                        MODULE_P00.OMR.U = (1u << 22);

                        break;


                    default:
                        break;
                }
            }
        }


        /**************** SAVE BUTTON STATE ****************/

        /*
         * Save current button state for the next
         * main-loop iteration.
         */
        previousButtonState = currentButtonState;
    }
}

