/*
 * Copyright (c) 2016-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== main_freertos.c ========
 */
#include <stdint.h>

#ifdef __ICCARM__
#include <DLib_Threads.h>
#endif

/* POSIX Header files */
#include <pthread.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>

#include <ti/drivers/GPIO.h>

/* Example/Board Header files */
#include "ti_drivers_config.h"
#include "eth_if.h"
#include "Ampr-queue.h"
#include "ethBuf.h"
#include "lcd_if.h"
#include "cmd_if.h"

#if defined(CONFIG_LP_CC2674R10_FPGA)
#include <ti/drivers/power/PowerCC26XX.h>
#include "ti/devices/DeviceFamily.h"
#include DeviceFamily_constructPath(driverlib/ioc.h)
#endif

extern void *mainThread(void *arg0);

/* Stack size in bytes */
#define THREADSTACKSIZE    2096
//#define THREADSTACKSIZE    3000

#define MAIN_PRIO 5
#define ETH_IF_PRIO 4
#define CMD_PRIO 3
#define LCD_PRIO 2

/*
 *  ======== main ========
 */
int main(void)

{
//    pthread_t           thread,thread1,thread2,threadNET;
//    pthread_attr_t      attrs;
//    TaskHandle_t        Radio_h,Eth_h,Cmd_h;
    struct sched_param  priParam;
    int                 retc;
    int                 detachState;

    /* initialize the system locks */
#ifdef __ICCARM__
    __iar_Initlocks();
#endif

    /* Call driver init functions */
    Board_initGeneral();
    GPIO_init();

    /* Set priority and stack size attributes */
//    pthread_attr_init(&attrs);
    priParam.sched_priority = 7;

//    detachState = PTHREAD_CREATE_DETACHED;
//    retc = pthread_attr_setdetachstate(&attrs, detachState);
//    if (retc != 0) {
        /* pthread_attr_setdetachstate() failed */
//        while (1);
//    }
//    pthread_attr_setschedparam(&attrs, &priParam);

//    retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
//    if (retc != 0) {
        /* pthread_attr_setstacksize() failed */
//        while (1);
//    }
    BaseType_t task_res = xTaskCreate(mainThread, "main", 600, NULL, MAIN_PRIO, NULL);
    if (task_res != pdPASS) {
        /* xTaskCreate() failed */
        while (1);
    }
//    attrs.priority = 3;
//    retc = pthread_create(&thread1, &attrs, Cmd, NULL);
//    if (retc != 0) {
//        /* pthread_create() failed */
//        while (1);
//    }
//    attrs.priority = 4;
//    retc = pthread_create(&thread2, &attrs, Eth, NULL);
////    xTaskCreate(Eth(), "Eth", 2100, (void *) 1,4,&Eth_h);
//    if (retc != 0) {
//        /* pthread_create() failed */
//        while (1);
//    }
//    attrs.priority = 6;
//    retc = pthread_create(&threadNET, &attrs, Net, NULL);
//    if (retc != 0) {
//        /* pthread_create() failed */
//        while (1);
//    }

#if defined(CONFIG_LP_CC2674R10_FPGA)
    Power_setConstraint(PowerCC26XX_IDLE_PD_DISALLOW);
    Power_setConstraint(PowerCC26XX_SB_DISALLOW);

    IOCPortConfigureSet(IOID_29, IOC_PORT_RFC_GPO0, IOC_IOMODE_NORMAL);
    IOCPortConfigureSet(IOID_30, IOC_PORT_RFC_GPI0, IOC_INPUT_ENABLE);
#endif

    task_res = xTaskCreate(vEthIf_task, "Eth IF", 600, NULL, ETH_IF_PRIO, NULL);
// and start the LCD task
    task_res = xTaskCreate(vLcdIf_task, "LCD", 600, NULL, LCD_PRIO, NULL);

    task_res = xTaskCreate(vCmdIf_task, "CMD", 600, NULL, CMD_PRIO, NULL);

    ethBuf_init();
    ampr_initQueue();

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    return (0);
}

//*****************************************************************************
//
//! \brief Application defined stack overflow hook
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    //Handle FreeRTOS Stack Overflow
    while(1)
    {
    }
}
