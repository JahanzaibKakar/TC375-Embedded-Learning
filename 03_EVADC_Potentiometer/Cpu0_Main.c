 * IN THE SOFTWARE.
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Ifx_Cfg_Ssw.h"
#include "IfxEvadc_Adc.h"  //Infineon iLLD interface for configuring and using EVADC as an ADC (driver).

IFX_ALIGN(4) IfxCpu_syncEvent cpuSyncEvent = 0;

/* Global EVADC module object.
 * Represents the complete TC375 EVADC peripheral in software.
 */
IfxEvadc_Adc g_evadc;


/* Global EVADC group object.
 * Will represent EVADC Group 0, which contains our potentiometer channel.
 */
IfxEvadc_Adc_Group g_evadcGroup;


/* Global EVADC channel object.
 * Will represent Group 0, Channel 0 (G0CH0), connected to AN0.
 */
IfxEvadc_Adc_Channel g_evadcChannel;

/* Global variable used to store the latest raw ADC conversion value */
volatile uint32 adcRawValue = 0;
volatile Ifx_EVADC_G_RES adcDebugResult;

/* ==================== EVADC Module INITIALIZATION ==================== */
//*Creates a local configuration variable that stores the EVADC module/settings before initialization. *//

void initEVADC(void)
{
    /* Temporary local variable used to store the desired (our) EVADC settings */
    IfxEvadc_Adc_Config evadcConfig;

    /* Lload the default EVADC module settings into evadcConfig */
    IfxEvadc_Adc_initModuleConfig(&evadcConfig, &MODULE_EVADC);  //Infineon  predefined EVADC starting settings and
    //store these setting our local config veraible//

    /* Apply those settings to the EVADC hardware and initialize g_evadc  */
    IfxEvadc_Adc_initModule(&g_evadc, &evadcConfig);
}

/* ==================== EVADC GROUP INITIALIZATION/Configuration ==================== */

void initEVADCGroup(void)
{
    /* Temporary local variable used to store the desired EVADC Group settings */
    IfxEvadc_Adc_GroupConfig evadcGroupConfig;

    /* Now first i have to Load Infineon's default/predefined EVADC group settings */
       IfxEvadc_Adc_initGroupConfig(&evadcGroupConfig, &g_evadc);

       //*Take Infineon default group settings store them in evadcGroupConfig *local vraible*,
      // and associate that configuration with our already-initialized EVADC module g_evadc.*//

       //* Select EVADC Group 0 *// sellection of group id from various group of VADC Module //
       evadcGroupConfig.groupId = IfxEvadc_GroupId_0;

       /* As here in our project Group 0 is the only group used, so it is also its own master */
       evadcGroupConfig.master = IfxEvadc_GroupId_0;

       /* Enable Queue 0 as the conversion request source */
       evadcGroupConfig.arbiter.requestSlotQueue0Enabled = TRUE;

       //* Queue 0 holds ADC conversion requests.
        //* A request asks the ADC to perform a conversion.
        //* The arbiter decides which enabled request source gets access to the ADC converter.
        /* Which request source gets to use the ADC converter now */
        //requestSlotQueue0Enabled = TRUE   <> “Arbiter, Queue 0 is allowed to request the ADC converter.”//
///* Modificatio........///
       /* Allow Queue 0 to issue a conversion request whenever a valid request is pending */
       evadcGroupConfig.queueRequest[0].triggerConfig.gatingMode = IfxEvadc_GatingMode_always;

        //=============Intilization=======//
        /* Now Apply the Group 0 configuration and initialize the EVADC group */
        IfxEvadc_Adc_initGroup(&g_evadcGroup, &evadcGroupConfig);

       //Take all the settings currently stored in evadcGroupConfig — Infineon defaults plus our changes such as Group 0,
       // Group 0 as master, and Queue 0 enabled — and apply them to the real EVADC Group 0 hardware.
}

        /* ==================== EVADC CHANNEL INITIALIZATION ==================== */

        void initEVADCChannel(void)
        {
            /* Temporary local variable used to store the desired Channel settings */
            IfxEvadc_Adc_ChannelConfig evadcChannelConfig;

            /* Load Infineon's default Channel settings for our initialized Group 0 */
            IfxEvadc_Adc_initChannelConfig(&evadcChannelConfig, &g_evadcGroup);

            /* Select Channel 0, which is connected to AN0 */
            evadcChannelConfig.channelId = IfxEvadc_ChannelId_0;

            /* Store the converted digital value in Result Register 0 */
            evadcChannelConfig.resultRegister = IfxEvadc_ChannelResult_0;

            /* Apply these settings and initialize Channel 0 */
            IfxEvadc_Adc_initChannel(&g_evadcChannel, &evadcChannelConfig);
        }

        /* ==================== EVADC QUEUE SETUP ==================== */

        /*
         * Queue 0 stores the conversion request for Channel 0.
         * Channel 0 is connected to AN0, where the onboard potentiometer provides the analog voltage.
         * By adding Channel 0 to Queue 0, we tell Group 0 that this channel should be converted.
         * IFXEVADC_QUEUE_REFILL keeps Channel 0 in the queue so it can be converted again and again.
         * it means after one conversion, keep Channel 0 ready for another conversion instead of removing it permanently from the queue.
         */
        void initEVADCQueue(void)
        {
            /* Add Channel 0 to Queue 0 so it can be requested for ADC conversion */
            IfxEvadc_Adc_addToQueue(&g_evadcChannel,
                                IfxEvadc_RequestSource_queue0,
                                    IFXEVADC_QUEUE_REFILL);


        }


        /* ==================== EVADC RESULT READING ==================== */

        void readEVADC(void)

        {

            /* Temporary variable representing the complete EVADC result register */

            // Ifx_EVADC_G_RES conversionResult;
            /* We are not using the local conversionResult variable anymore.
               Instead, adcDebugResult is a global volatile variable so we can inspect it
               easily in the debugger. */

            /* Ifx_EVADC_G_RES is Infineon's special data type matching an EVADC Group Result Register.

               adcDebugResult is our global debug variable that holds the complete ADC Result Register Information. */

            /* Keep reading Result Register 0 until a valid/new ADC result is available */

            do

            {

                adcDebugResult = IfxEvadc_Adc_getResult(&g_evadcChannel);

                /* IfxEvadc_Adc_getResult() is an Infineon iLLD function that reads the ADC conversion result of the selected EVADC channel0. */

            }

            while(adcDebugResult.B.VF == 0);

            /* Store only the actual converted ADC number in adcRawValue */

            adcRawValue = adcDebugResult.B.RESULT;

        }
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
    
    /* ==================== EVADC FUNCTION CALLS ==================== */

    /* 1 Initialize the complete EVADC module */
    initEVADC();

    /* 2 Initialize EVADC Group 0 */
    initEVADCGroup();

    /* 3 Initialize Channel 0 connected to AN0 */
    initEVADCChannel();

    /* 4 Add Channel 0 conversion request to Queue 0 */
    initEVADCQueue();
    
//    Start Queue 0 now.

    IfxEvadc_Adc_startQueue(&g_evadcGroup, IfxEvadc_RequestSource_queue0);
    /* Start Queue 0 so Group 0 begins ADC conversion requests */
  // begin the actual ADC conversion process for the requests stored in Queue 0./


    while(1)
    {

            /* Continuously read the latest ADC conversion result from Channel 0 */
            readEVADC();
    }

}