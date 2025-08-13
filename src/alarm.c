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
 *   This file implements the OpenThread platform abstraction for the alarm.
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform-bee.h"
#include "mac_driver.h"
#include "mac_driver_mpan.h"
#include "common/logging.hpp"
#include <openthread/config.h>
#include <openthread/platform/alarm-micro.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/diag.h>
#include <openthread/platform/time.h>

void BEE_AlarmInit(void)
{
}

//volatile bool micro_alarm_fired[MAX_PAN_NUM];
volatile uint64_t micro_alarm_us[MAX_PAN_NUM];
volatile uint32_t micro_alarm_btclk_us[MAX_PAN_NUM];

APP_RAM_TEXT_SECTION void micro_handler(uint8_t pan_idx)
{
    if (micro_alarm_us[pan_idx] > 0)
    {
        BEE_EventSend(ALARM_US, pan_idx);
        if (pan_idx == 0) { otSysEventSignalPending(); }
        else { zbSysEventSignalPending(); }
    }
}

uint32_t otPlatAlarmMicroGetNow(void)
{
    return (uint32_t)otPlatTimeGet();
}

void otPlatAlarmMicroStartAt(otInstance *aInstance, uint32_t t0, uint32_t dt)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    uint64_t now;
    uint32_t curr_us;
    uint64_t target_us;
    uint32_t diff;
    uint32_t s = os_lock();
    curr_us = mac_GetCurrentBTUS();
    now = bt_clk_offset + curr_us;
    os_unlock(s);
    target_us = mac_ConvertT0AndDtTo64BitTime(t0, dt, &now);
    if (pan_idx == 0)
    {
        if (target_us > now)
        {
            micro_alarm_us[pan_idx] = target_us;
            diff = target_us - now;
            micro_alarm_btclk_us[pan_idx] = (curr_us + diff) % MAX_BT_CLOCK_COUNTER;
            mac_SetBTClkUSInt(MAC_BT_TIMER1, micro_alarm_btclk_us[pan_idx]);
        }
        else
        {
            micro_alarm_us[pan_idx] = 0;
            micro_alarm_btclk_us[pan_idx] = 0;
            BEE_EventSend(ALARM_US, pan_idx);
        }
    }
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
    else
    {
        if (target_us > now)
        {
            micro_alarm_us[pan_idx] = target_us;
            diff = target_us - now;
            micro_alarm_btclk_us[pan_idx] = (curr_us + diff) % MAX_BT_CLOCK_COUNTER;
            mac_SetBTClkUSInt(MAC_BT_TIMER5, micro_alarm_btclk_us[pan_idx]);
        }
        else
        {
            micro_alarm_us[pan_idx] = 0;
            micro_alarm_btclk_us[pan_idx] = 0;
            BEE_EventSend(ALARM_US, pan_idx);
        }
    }
#endif
}

void otPlatAlarmMicroStop(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    micro_alarm_us[pan_idx] = 0;
    micro_alarm_btclk_us[pan_idx] = 0;
}

//volatile bool milli_alarm_fired[MAX_PAN_NUM];
volatile uint64_t milli_alarm_us[MAX_PAN_NUM];
volatile uint32_t milli_alarm_btclk_us[MAX_PAN_NUM];

APP_RAM_TEXT_SECTION void milli_handler(uint8_t pan_idx)
{
    if (milli_alarm_us[pan_idx] > 0)
    {
        BEE_EventSend(ALARM_MS, pan_idx);
        if (pan_idx == 0) { otSysEventSignalPending(); }
        else { zbSysEventSignalPending(); }
    }
}

uint32_t otPlatAlarmMilliGetNow(void)
{
    return (uint32_t)(otPlatTimeGet() / 1000);
}

void otPlatAlarmMilliStartAt(otInstance *aInstance, uint32_t t0, uint32_t dt)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    uint64_t now;
    uint32_t curr_us;
    uint64_t now_ms;
    uint64_t target_ms = mac_ConvertT0AndDtTo64BitTime(t0, dt, &now_ms);
    uint32_t diff_ms;
    uint64_t target_us;
    uint32_t diff;
    uint32_t s = os_lock();
    curr_us = mac_GetCurrentBTUS();
    now = bt_clk_offset + curr_us;
    os_unlock(s);
    now_ms = now / 1000;
    target_ms = mac_ConvertT0AndDtTo64BitTime(t0, dt, &now_ms);
    if (pan_idx == 0)
    {
        if (target_ms > now_ms)
        {
            diff_ms = target_ms - now_ms;
            diff = diff_ms * 1000;
            target_us = now + diff;
            milli_alarm_us[pan_idx] = target_us;
            milli_alarm_btclk_us[pan_idx] = (curr_us + diff) % MAX_BT_CLOCK_COUNTER;
            mac_SetBTClkUSInt(MAC_BT_TIMER0, milli_alarm_btclk_us[pan_idx]);
        }
        else
        {
            milli_alarm_us[pan_idx] = 0;
            milli_alarm_btclk_us[pan_idx] = 0;
            BEE_EventSend(ALARM_MS, pan_idx);
        }
    }
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
    else
    {
        if (target_us > now)
        {
            diff_ms = target_ms - now_ms;
            diff = diff_ms * 1000;
            target_us = now + diff;
            milli_alarm_us[pan_idx] = target_us;
            milli_alarm_btclk_us[pan_idx] = (curr_us + diff) % MAX_BT_CLOCK_COUNTER;
            mac_SetBTClkUSInt(MAC_BT_TIMER4, milli_alarm_btclk_us[pan_idx]);
        }
        else
        {
            milli_alarm_us[pan_idx] = 0;
            milli_alarm_btclk_us[pan_idx] = 0;
            BEE_EventSend(ALARM_MS, pan_idx);
        }
    }
#endif
}

void otPlatAlarmMilliStop(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    milli_alarm_us[pan_idx] = 0;
    milli_alarm_btclk_us[pan_idx] = 0;
}

#define XTAL_ACCURACY 40
uint16_t otPlatTimeGetXtalAccuracy(void)
{
#if DLPS_EN
    return XTAL_ACCURACY;
#endif
    return 10;
}

uint64_t otPlatTimeGet(void)
{
    uint64_t now;
    uint32_t s = os_lock();
    now = bt_clk_offset + mac_GetCurrentBTUS();
    os_unlock(s);
    return now;
}
