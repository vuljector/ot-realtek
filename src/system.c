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

void otSysProcessDrivers(otInstance *aInstance)
{
    BEE_UartRx();
    BEE_UartTx();
    BEE_EventProcess(aInstance, 0);
    BEE_AlarmProcess(aInstance, 0);
}

#ifdef BUILD_MATTER
#else
extern void *thread_task_handle;

void otSysEventSignalPending(void)
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
    ZBOSS_UartRx();
    ZBOSS_UartTx();
    BEE_EventProcess(aInstance, 1);
    BEE_AlarmProcess(aInstance, 1);
}

extern void *zigbee_task_handle;

void zbSysEventSignalPending(void)
{
    os_task_notify_give(zigbee_task_handle);
}

void zbTaskletsSignalPending(otInstance *aInstance)
{
    os_task_notify_give(zigbee_task_handle);
}
