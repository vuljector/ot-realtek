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
 *   OpenThread platform abstraction for the alarm.
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

//------------------------------------------------------------------------------
// Configuration / forward decls
//------------------------------------------------------------------------------

#define EXTRA_PROTECTION_EN 0

extern void pm_check_inactive(const char *callerStr, bool wakeup);

// Minimum ahead-of-time (in us) required to program HW timer; otherwise, fire immediately.
static const uint32_t kMinScheduleAdvanceUs = 20;

//------------------------------------------------------------------------------
// State
//------------------------------------------------------------------------------

volatile uint64_t micro_alarm_us[MAX_PAN_NUM];
volatile uint64_t milli_alarm_us[MAX_PAN_NUM];

//------------------------------------------------------------------------------
// Common helpers
//------------------------------------------------------------------------------

typedef enum
{
    kAlarmMicro = 0,
    kAlarmMilli = 1,
} AlarmKind;

/**
 * Select HW timer ID based on PAN index and alarm kind.
 */
static inline uint32_t SelectTimerId(uint8_t pan_idx, AlarmKind kind)
{
    (void)kind;

#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
    // Multi-PAN supported: use different timers per PAN/kind.
    if (pan_idx > 0)
    {
        return (kind == kAlarmMicro) ? MAC_BT_TIMER5 : MAC_BT_TIMER4;
    }
    else
    {
        return (kind == kAlarmMicro) ? MAC_BT_TIMER1 : MAC_BT_TIMER0;
    }
#else
    // Single-PAN platforms: only PAN 0 is valid.
    if (pan_idx > 0)
    {
        return UINT32_MAX; // invalid; caller should bail out
    }
    return (kind == kAlarmMicro) ? MAC_BT_TIMER1 : MAC_BT_TIMER0;
#endif
}

/**
 * Fire the corresponding alarm event immediately.
 */
static inline void FireAlarmNow(AlarmKind kind, uint8_t pan_idx)
{
    BEE_EventSend((kind == kAlarmMicro) ? ALARM_US : ALARM_MS, pan_idx);
    if (pan_idx == 0)
    {
        otSysEventSignalPending();
    }
    else
    {
        zbSysEventSignalPending();
    }
}

/**
 * Schedule an alarm or fire immediately if too close.
 * Returns true if scheduled; false if fired immediately.
 */
static bool ScheduleOrFire(AlarmKind kind,
                           otInstance *aInstance,
                           uint8_t     pan_idx,
                           uint64_t    target_us,
                           uint64_t    now_us,
                           uint32_t    curr_hw_us)
{
    uint32_t diff = 0;

    if (target_us > now_us)
    {
        uint64_t delta = target_us - now_us;
        diff           = (delta > UINT32_MAX) ? UINT32_MAX : (uint32_t)delta;
    }

    if (diff > kMinScheduleAdvanceUs)
    {
        const uint32_t tid = SelectTimerId(pan_idx, kind);
#if !(defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D))
        if (tid == UINT32_MAX)
        {
            // Unsupported PAN; nothing to do on single-PAN platforms.
            return false;
        }
#endif
        extern const uint8_t kTxBackoffTmrMode;
        if ((kTxBackoffTmrMode == 1 && kind == kAlarmMicro) ||
            (kTxBackoffTmrMode == 0 && kind == kAlarmMilli))
        {
            extern uint64_t get_tx_backoff_pending(uint8_t pan_idx);
            uint64_t tx_backoff_pending = get_tx_backoff_pending(pan_idx);
            if (tx_backoff_pending && tx_backoff_pending < target_us)
            {
                return false;
            }
        }
        mac_SetBTClkUSInt(tid, curr_hw_us + diff);
        // otLogNotePlat("%s set %u us", (kind == kAlarmMicro) ? "us" : "ms", diff);
        BEE_SleepProcess(aInstance, pan_idx);
        return true;
    }

    // Too close in time: fire immediately.
    FireAlarmNow(kind, pan_idx);
    return false;
}

//------------------------------------------------------------------------------
// Platform API
//------------------------------------------------------------------------------

void BEE_AlarmInit(void) {}

APP_RAM_TEXT_SECTION void micro_handler(uint8_t pan_idx)
{
    // Guard: only signal if an alarm is armed for this PAN.
    if (micro_alarm_us[pan_idx] > 0)
    {
        FireAlarmNow(kAlarmMicro, pan_idx);
    }
}

uint32_t otPlatAlarmMicroGetNow(void)
{
    // Truncate to 32-bit microseconds as per OT platform API.
    return (uint32_t)otPlatTimeGet();
}

void otPlatAlarmMicroStartAt(otInstance *aInstance, uint32_t t0, uint32_t dt)
{
    const uint8_t pan_idx = mpan_GetCurrentPANIdx();

#if !(defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D))
    // Single-PAN platforms only support PAN 0.
    if (pan_idx > 0)
    {
        return;
    }
#endif

#if EXTRA_PROTECTION_EN
    pm_check_inactive("otPlatAlarmMicroStartAt", true);
#endif

    uint32_t s       = os_lock();
    uint32_t curr_us = mac_GetCurrentBTUS();
    uint64_t now_us  = bt_clk_offset + curr_us;

    // Convert (t0, dt) to absolute 64-bit microsecond timestamp.
    uint64_t target_us = mac_ConvertT0AndDtTo64BitTime(t0, dt, &now_us);
    micro_alarm_us[pan_idx] = target_us;

    (void)ScheduleOrFire(kAlarmMicro, aInstance, pan_idx, target_us, now_us, curr_us);
    os_unlock(s);
}

void otPlatAlarmMicroStop(otInstance *aInstance)
{
    const uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    // Disarm: handler will ignore spurious IRQs afterwards.
    micro_alarm_us[pan_idx] = 0;
    BEE_SleepProcess(aInstance, pan_idx);
}

APP_RAM_TEXT_SECTION void milli_handler(uint8_t pan_idx)
{
    if (milli_alarm_us[pan_idx] > 0)
    {
        FireAlarmNow(kAlarmMilli, pan_idx);
    }
}

uint32_t otPlatAlarmMilliGetNow(void)
{
    // Convert microseconds to milliseconds as per API contract.
    return (uint32_t)(otPlatTimeGet() / 1000);
}

void otPlatAlarmMilliStartAt(otInstance *aInstance, uint32_t t0, uint32_t dt)
{
    const uint8_t pan_idx = mpan_GetCurrentPANIdx();

#if !(defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D))
    if (pan_idx > 0)
    {
        return;
    }
#endif

#if EXTRA_PROTECTION_EN
    pm_check_inactive("otPlatAlarmMilliStartAt", true);
#endif

    uint32_t s       = os_lock();
    uint32_t curr_us = mac_GetCurrentBTUS();
    uint64_t now_us  = bt_clk_offset + curr_us;

    // For milli alarm, original logic converts using millisecond base.
    uint64_t now_ms    = now_us / 1000;
    uint64_t target_ms = mac_ConvertT0AndDtTo64BitTime(t0, dt, &now_ms);
    uint64_t target_us = target_ms * 1000;

    milli_alarm_us[pan_idx] = target_us;

    (void)ScheduleOrFire(kAlarmMilli, aInstance, pan_idx, target_us, now_us, curr_us);
    os_unlock(s);
}

void otPlatAlarmMilliStop(otInstance *aInstance)
{
    const uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    milli_alarm_us[pan_idx] = 0;
    BEE_SleepProcess(aInstance, pan_idx);
}

uint16_t otPlatTimeGetXtalAccuracy(void)
{
#if DLPS_EN
    return 255;
#else
    return 10;
#endif
}

uint64_t otPlatTimeGet(void)
{
    uint32_t s = os_lock();
    uint64_t now = bt_clk_offset + mac_GetCurrentBTUS();
    os_unlock(s);
    return now;
}
