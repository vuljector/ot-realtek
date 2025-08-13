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
 *   This file includes the platform-specific initializers.
 *
 */

#ifndef PLATFORM_BEE4_H_
#define PLATFORM_BEE4_H_

#include <openthread-core-config.h>
#include <stdint.h>
#include <openthread/config.h>
#include <openthread/instance.h>
#include <openthread/tasklet.h>
#include "openthread-system.h"
#include "rtl_pinmux.h"
#include "rtl_rcc.h"
#include "rtl_gpio.h"
#include "rtl_uart.h"
#include "rtl_gdma.h"
#include "rtl_nvic.h"
#include "ftl.h"
#include "flash_nor_device.h"
#include "os_task.h"
#include "os_mem.h"
#include "os_sync.h"
#include "os_sched.h"
#include "os_msg.h"
#include "utils.h"
#include "gap.h"
#include "trace.h"
#include "vector_table_ext.h"
#include "flash_map.h"
#include "power_manager_unit_zbmac.h"
#include "wdt.h"
#include "dbg_printf.h"
#include "rtl876x_lib_platform.h"
#include "app_section.h"

#if (BUILD_RCP == 1)
#define RX_BUF_SIZE     16
#define EV_BUF_SIZE     16
#else
#define RX_BUF_SIZE     8
#define EV_BUF_SIZE     8
#endif

#define NONE		0
#define RX_OK		1
#define TX_DONE		2
#define ED_SCAN		3
#define UART_RX		4
#define UART_TX		5
#define ALARM_US	6
#define ALARM_MS	7
#define SLEEP		8
#define WAKEUP		9

void BEE_EventSend(uint8_t event, uint8_t pan_idx);

/**
 * This function initializes the alarm service used by OpenThread.
 *
 */
void BEE_AlarmInit(void);

/**
 * This function performs alarm driver processing.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
void BEE_AlarmMicroProcess(otInstance *aInstance, uint8_t pan_idx);
void BEE_AlarmMilliProcess(otInstance *aInstance, uint8_t pan_idx);

/**
 * This function initializes the radio service used by OpenThread.
 *
 */
void BEE_RadioInit(uint8_t pan_idx);
void BEE_SleepDirect(void);

/**
 * This function performs radio driver processing.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
void BEE_RadioRx(otInstance *aInstance, uint8_t pan_idx);
void BEE_RadioTx(otInstance *aInstance, uint8_t pan_idx);
void BEE_RadioEnergyScan(otInstance *aInstance, uint8_t pan_idx);
void BEE_SleepProcess(otInstance *aInstance, uint8_t pan_idx);
void BEE_WakeupProcess(otInstance *aInstance, uint8_t pan_idx);

/**
 * This function initializes the random number service used by OpenThread.
 *
 */
void BEE_RandomInit(void);

/**
 * This function performs UART driver processing.
 *
 */
void BEE_UartRx(void);
void BEE_UartTx(void);

void ZBOSS_UartRx(void);
void ZBOSS_UartTx(void);

#define OT_UART_TX                                     P3_0
#define OT_UART_RX                                     P3_1

#define OT_UART         UART3
#define OT_UART_APB     APBPeriph_UART3
#define OT_UART_APBCLK  APBPeriph_UART3_CLOCK
#define OT_UART_IRQN    UART3_IRQn
#define OT_UART_VECTORn UART3_VECTORn
#define OT_UARTIntHandler UART3_Handler

#define OT_US_TMR_SRC   0   // BT clock timer ID for OT micro-second timer source
#define OT_MS_TMR_SRC   1   // BT clock timer ID for OT mini-second timer source

typedef uint8_t bool_t;
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;
typedef int16_t s16_t;
typedef int32_t s32_t;
typedef int64_t s64_t;


/**
 * This function sets the internal timer.
 *
 * @param[in]   start  The timer start time.
 * @param[in]   duration  The timer duration.
 *
 */
extern volatile uint64_t bt_clk_offset;
void milli_handler(uint8_t pan_idx);
void micro_handler(uint8_t pan_idx);

extern void mac_GrantPHYFast(void);
extern bool_t mac_GrantPHYStatus(void);
extern void mac_GrantPHYAdjustForBLE(void);
extern void mac_GrantPHYAdjustForZB(void);
extern void mac_SetAddrMatchMode_patch(uint8_t mode);
extern uint32_t mac_SetTxNCsmaDetail(bool enable, uint8_t be);
extern uint8_t mac_LoadTxNPayload_patch(uint8_t HdrL, uint8_t FrmL, uint8_t *TxFIFO);
extern void mac_TrigUpperEnc_patch(void);
extern uint8_t mpan_TrigTxNAtTime_patch(bool_t AckReq, bool_t SecReq, bool_t DoCCA, uint32_t target_us, uint8_t pan_idx);
extern uint32_t mac_BackoffDelay(uint8_t nb);
extern void mac_GivenTimeDelay(uint64_t target_us);
extern uint8_t mac_TrigTxEnhAck_patch(bool_t early, bool_t SecReq);

typedef void (*bt_hci_reset_handler_t)(void);
extern void mac_RegisterBtHciResetHanlder(bt_hci_reset_handler_t handler);

extern int8_t mac_SetTXPower_patch(int8_t tx_dbm);
extern int8_t mac_GetTXPower_patch(void);
extern void mac_EDScan_begin(uint32_t scan_round);
extern void mac_EDScan_end(uint8_t *peak_value);
extern bool mac_GetTxNTermedStatus(void);
extern uint32_t mac_GetTxMStatus(void);
extern void mac_PTA_Wrokaround(void);
int8_t mac_edscan_level2dbm(uint8_t level);
uint8_t mac_edscan_dbm2level(int8_t dbm);
uint8_t mac_GetCcaEDThreshold_patch(void);
uint8_t mac_GetCurrentCCAReport(void);

void zbSysInit(int argc, char *argv[]);
bool zbSysPseudoResetWasRequested(void);
void zbSysProcessDrivers(otInstance *aInstance);
void zbSysEventSignalPending(void);
void zbTaskletsSignalPending(otInstance *aInstance);

#define APP_WAKEUP_REASON_NONE          0
#define APP_WAKEUP_REASON_BUTTON_SW1    1
#define APP_WAKEUP_REASON_BUTTON_SW2    2
#define APP_WAKEUP_REASON_BUTTON_SW3    3
#define APP_WAKEUP_REASON_BUTTON_SW4    4
#define APP_WAKEUP_REASON_UART_RX       5
extern volatile uint32_t app_wakeup_reason;

#endif // PLATFORM_SBEE2_H_
