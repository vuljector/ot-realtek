/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief
 *   This file includes the platform-specific initializers.
 */
#include "platform-bee.h"
#include "mac_driver.h"
#include "mac_driver_mpan.h"
#include <openthread/config.h>
#include "common/logging.hpp"
#include "openthread-system.h"

#if OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2
#if OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
void *SBee2CAlloc(size_t aNum, size_t aSize)
{
    //return calloc(aNum, aSize);
    return os_mem_zalloc(RAM_TYPE_DATA_ON, aNum * aSize);
}

void SBee2Free(void *aPtr)
{
    //free(aPtr);
    os_mem_free(aPtr);
}
#endif
#endif

extern void zbmac_pm_initiate_wakeup(void);
APP_RAM_TEXT_SECTION void BEE_RadioExternalWakeup(void)
{
    zbmac_pm_initiate_wakeup();
}

typedef struct
{
    volatile uint16_t ev_tail;
    volatile uint16_t ev_head;
    uint8_t ev_queue[EV_BUF_SIZE];
} ev_item_t;
static ev_item_t ev_item[MAX_PAN_NUM];

APP_RAM_TEXT_SECTION void BEE_EventSend(uint8_t event, uint8_t pan_idx)
{
    uint32_t s = os_lock();
    ev_item[pan_idx].ev_queue[ev_item[pan_idx].ev_tail] = event;
    ev_item[pan_idx].ev_tail = (ev_item[pan_idx].ev_tail + 1) % EV_BUF_SIZE;
    os_unlock(s);
}

int gPlatformPseudoResetLevel = 0;

void otSysInit(int argc, char *argv[])
{
    gPlatformPseudoResetLevel = 0;

    OT_UNUSED_VARIABLE(argc);
    OT_UNUSED_VARIABLE(argv);
#if OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2
#if OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
    otHeapSetCAllocFree(SBee2CAlloc, SBee2Free);
#endif
#endif

    BEE_RadioInit(0);
    BEE_AlarmInit();
    BEE_RandomInit();
}

bool otSysPseudoResetWasRequested(void)
{
    uint32_t notify;
    if (gPlatformPseudoResetLevel > 0)
    {
        return true;
    }
    else
    {
        os_task_notify_take(1, 0xffffffff, &notify);
        return false;
    }
}

#if (ENABLE_CLI == 1)
#include <openthread/cli.h>
#include "common/code_utils.hpp"

static volatile bool user_commands_registered = false;

#if (XMODEM_ENABLE == 1)
static otError ProcessFirmwareUpdate(void *aContext, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aContext);
    OT_UNUSED_VARIABLE(aArgsLength);
    OT_UNUSED_VARIABLE(aArgs);

    uint32_t boot_flag = 0xdeadbeef;
    ftl_save_to_module("app", &boot_flag, 0, sizeof(boot_flag));
    WDG_SystemReset(RESET_ALL, SW_RESET_APP_END);
    return OT_ERROR_NONE;
}
#endif
#if defined(DLPS_EN) && (DLPS_EN == 1)
static otError ProcessSleepDirect(void *aContext, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aContext);
    OT_UNUSED_VARIABLE(aArgsLength);
    OT_UNUSED_VARIABLE(aArgs);
    BEE_SleepDirect();
    return OT_ERROR_NONE;
}
#if defined(BUILD_MATTER) || defined(BUILD_BLE_PERIPHERAL)
#if defined(BUILD_MATTER)
extern void SetMatterBLEAdvEnabled(bool val);
#endif
static otError ProcessBleAdvState(void *aContext, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aContext);
    if (aArgsLength == 1)
    {
        if (strncmp(aArgs[0], "dis", 3) == 0)
        {
#if defined(BUILD_BLE_PERIPHERAL)
            le_adv_stop();
#else
            SetMatterBLEAdvEnabled(false);
#endif
        }
        else if (strncmp(aArgs[0], "en", 2) == 0)
        {
#if defined(BUILD_BLE_PERIPHERAL)
            le_adv_start();
#else
            SetMatterBLEAdvEnabled(true);
#endif
        }
        else
        {
            otCliOutputFormat("invalid arg");
        }
    }
    return OT_ERROR_NONE;
}
#endif
#if defined(BUILD_MATTER)
extern void matter_gpio_allow_to_enter_dlps(void);
extern void SetSystemLedState(bool state);
static otError ProcessSystemLedState(void *aContext, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aContext);
    if (aArgsLength == 1)
    {
        if (strncmp(aArgs[0], "off", 3) == 0)
        {
            matter_gpio_allow_to_enter_dlps();
            SetSystemLedState(false);
        }
        else if (strncmp(aArgs[0], "on", 2) == 0)
        {
            SetSystemLedState(true);
        }
        else
        {
            otCliOutputFormat("invalid arg");
        }
    }
    return OT_ERROR_NONE;
}
#endif
#endif

static const otCliCommand rtkCommands[] =
{
#if (XMODEM_ENABLE == 1)
    {"fwupdate", ProcessFirmwareUpdate},
#endif
#if defined(DLPS_EN) && (DLPS_EN == 1)
    {"sleepdirect", ProcessSleepDirect},
#if defined(BUILD_MATTER) || defined(BUILD_BLE_PERIPHERAL)
    {"bleadv", ProcessBleAdvState},
#endif
#if defined(BUILD_MATTER)
    {"systemled", ProcessSystemLedState},
#endif
#endif
};
#endif

void otSysProcessDrivers(otInstance *aInstance)
{
    uint8_t event;

#if (ENABLE_CLI == 1)
    if (!user_commands_registered)
    {
        user_commands_registered = true;
        otCliSetUserCommands(rtkCommands, OT_ARRAY_LENGTH(rtkCommands), aInstance);
    }
#endif
    BEE_RadioBackoffTimeout(aInstance, 0);
    BEE_RadioRx(aInstance, 0);

    while (ev_item[0].ev_head != ev_item[0].ev_tail)
    {
        event = ev_item[0].ev_queue[ev_item[0].ev_head];
        switch (event)
        {
        case TX_START:
            BEE_RadioTxStart(aInstance, 0);
            break;

        case TX_DONE:
            BEE_RadioTx(aInstance, 0);
            break;

        case ED_SCAN:
            BEE_RadioEnergyScan(aInstance, 0);
            break;

        case UART_RX:
            BEE_UartRx();
            break;

        case UART_TX:
            BEE_UartTx();
            break;

        case ALARM_US:
            BEE_AlarmMicroProcess(aInstance, 0);
            break;

        case ALARM_MS:
            BEE_AlarmMilliProcess(aInstance, 0);
            break;

        case SLEEP:
            BEE_SleepProcess(aInstance, 0);
            break;

        case WAKEUP:
            BEE_WakeupProcess(aInstance, 0);
            break;

        default:
            break;
        }
        ev_item[0].ev_head = (ev_item[0].ev_head + 1) % EV_BUF_SIZE;
    }
}

#ifdef BUILD_MATTER
#else
extern void *thread_task_handle;

APP_RAM_TEXT_SECTION void otSysEventSignalPending(void)
{
    os_task_notify_give(thread_task_handle);
}

void __wrap_otTaskletsSignalPending(otInstance *aInstance)
{
    os_task_notify_give(thread_task_handle);
}
#endif

int gPlatformPseudoResetLevel_zb = 0;

void zbSysInit(int argc, char *argv[])
{
    gPlatformPseudoResetLevel_zb = 0;

    OT_UNUSED_VARIABLE(argc);
    OT_UNUSED_VARIABLE(argv);
#if OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2
#if OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
    otHeapSetCAllocFree(SBee2CAlloc, SBee2Free);
#endif
#endif

    BEE_RadioInit(1);
    BEE_AlarmInit();
    BEE_RandomInit();
}

bool zbSysPseudoResetWasRequested(void)
{
    uint32_t notify;
    if (gPlatformPseudoResetLevel_zb > 0)
    {
        return true;
    }
    else
    {
        os_task_notify_take(1, 0xffffffff, &notify);
        return false;
    }
}

void zbSysProcessDrivers(otInstance *aInstance)
{
    uint8_t event;

#if (ENABLE_CLI == 1 && XMODEM_ENABLE == 1)
    if (!user_commands_registered)
    {
        user_commands_registered = true;
        otCliSetUserCommands(rtkCommands, OT_ARRAY_LENGTH(rtkCommands), aInstance);
    }
#endif
    BEE_RadioBackoffTimeout(aInstance, 1);
    BEE_RadioRx(aInstance, 1);

    while (ev_item[1].ev_head != ev_item[1].ev_tail)
    {
        event = ev_item[1].ev_queue[ev_item[1].ev_head];
        switch (event)
        {
        case TX_START:
            BEE_RadioTxStart(aInstance, 1);
            break;

        case TX_DONE:
            BEE_RadioTx(aInstance, 1);
            break;

        case ED_SCAN:
            BEE_RadioEnergyScan(aInstance, 1);
            break;

        case UART_RX:
            BEE_UartRx();
            break;

        case UART_TX:
            BEE_UartTx();
            break;

        case ALARM_US:
            BEE_AlarmMicroProcess(aInstance, 1);
            break;

        case ALARM_MS:
            BEE_AlarmMilliProcess(aInstance, 1);
            break;

        case SLEEP:
            BEE_SleepProcess(aInstance, 1);
            break;

        case WAKEUP:
            BEE_WakeupProcess(aInstance, 1);
            break;

        default:
            break;
        }
        ev_item[1].ev_head = (ev_item[1].ev_head + 1) % EV_BUF_SIZE;
    }
}

extern void *zigbee_task_handle;

APP_RAM_TEXT_SECTION void zbSysEventSignalPending(void)
{
    os_task_notify_give(zigbee_task_handle);
}
