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

#ifndef ZB_TST_H_
#define ZB_TST_H_

#include <stdint.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define RTK_8771GTV_RCP_IC_TYPE 0x4754
#define RTK_8771GUV_RCP_IC_TYPE 0x4755
#define RTK_8777G_RCP_IC_TYPE 0x4755

#ifdef BOARD_RTL8771GUV
#define RTK_RCP_IC_TYPE RTK_8771GUV_RCP_IC_TYPE
#elif defined(BOARD_RTL8771GTV)
#define RTK_RCP_IC_TYPE RTK_8771GTV_RCP_IC_TYPE
#else
#define RTK_RCP_IC_TYPE RTK_8777G_RCP_IC_TYPE
#endif

#define HW_FLOWCTL_OFFSET         0
#define UART_TRX_LOG_OFFSET       1
#define FORCE_HW_FLOWCTL_OFFSET   7

#define BIT0        0x00000001
#define BIT1        0x00000002
#define BIT2        0x00000004
#define BIT3        0x00000008
#define BIT4        0x00000010
#define BIT5        0x00000020
#define BIT6        0x00000040
#define BIT7        0x00000080
#define BIT8        0x00000100
#define BIT9        0x00000200
#define BIT10       0x00000400
#define BIT11       0x00000800
#define BIT12       0x00001000
#define BIT13       0x00002000
#define BIT14       0x00004000
#define BIT15       0x00008000
#define BIT16       0x00010000
#define BIT17       0x00020000
#define BIT18       0x00040000
#define BIT19       0x00080000
#define BIT20       0x00100000
#define BIT21       0x00200000
#define BIT22       0x00400000
#define BIT23       0x00800000
#define BIT24       0x01000000
#define BIT25       0x02000000
#define BIT26       0x04000000
#define BIT27       0x08000000
#define BIT28       0x10000000
#define BIT29       0x20000000
#define BIT30       0x40000000
#define BIT31       0x80000000

typedef struct _rtk_config_param
{
    uint32_t sign;
    uint32_t vid: 16;
    uint32_t pid: 16;
    uint32_t tx_pin: 8;
    uint32_t rx_pin: 8;
    uint32_t rts_pin: 8;
    uint32_t cts_pin: 8;
    uint32_t func_msk: 8;
    uint32_t baud_rate: 24;
    uint32_t pta_dis: 8;
    uint32_t pta_wl_act: 8;
    uint32_t pta_bt_act: 8;
    uint32_t pta_bt_stat: 8;
    uint32_t pta_bt_clk: 8;
    uint32_t ext_pa: 8;
    uint32_t ext_lna: 8;
    uint32_t ext_pa_lna_ploatiry: 8;
    uint32_t ic_type: 16;
    uint32_t reserved: 16;
} __attribute__((packed)) rtk_config_param;
extern rtk_config_param config_param;

typedef enum rtk_rcp_baudrate_e
{
    RCP_BAUDRATE_115200 = 115200,
    RCP_BAUDRATE_230400 = 230400,
    RCP_BAUDRATE_460800 = 460800,
    RCP_BAUDRATE_921600 = 921600,
    RCP_BAUDRATE_1000000 = 1000000,
    RCP_BAUDRATE_2000000 = 2000000,
} rtk_rcp_baudrate_t;

typedef enum rtk_config_data_check_result_e
{
    CONFIG_DATA_NO_ERROR,
    CONFIG_DATA_IC_TYPE_ERROR = BIT0,
    CONFIG_DATA_BAUDRATE_ERROR = BIT1,
    CONFIG_DATA_PID_VID_ERROR = BIT2,
    CONFIG_DATA_PA_PIN_ERROR = BIT3,
    CONFIG_DATA_LNA_PIN_ERROR = BIT4,
} rtk_config_data_check_result_t;

#define ZB_DBG_UART                         UART2
#define ZB_DBG_UART_TX                      UART2_TX
#define ZB_DBG_UART_RX                      UART2_RX
#define ZB_DBG_UART_IRQn                    UART2_IRQn
#define ZB_DBG_UART_VECTORn                 UART2_VECTORn
#define ZB_CLI_UART                         UART3
#define ZB_CLI_UART_TX                      UART3_TX
#define ZB_CLI_UART_RX                      UART3_RX
#define ZB_CLI_UART_RTS                     UART3_RTS
#define ZB_CLI_UART_CTS                     UART3_CTS
#define ZB_CLI_UART_APB                     APBPeriph_UART3
#define ZB_CLI_UART_APBCLK                  APBPeriph_UART3_CLOCK
#define ZB_CLI_UART_IRQn                    UART3_IRQn
#define ZB_CLI_UART_VECTORn                 UART3_VECTORn
#define ZB_CLI_UARTIntHandler               UART3_Handler

#define ZB_TIM                              TIM2

// Define UART PIN Mux
#define ZB_DBG_UART_TX_PIN                  P2_4
#define ZB_DBG_UART_RX_PIN                  P2_5

#define ZB_CLI_UART_TX_PIN                  P3_0
#define ZB_CLI_UART_RX_PIN                  P3_1

#define ZB_RESET_PIN                        P0_5
#define ZB_INTERRUPT_PIN                    P0_4

#ifdef BUILD_USB
#define THREAD_TASK_PRIORITY             3        //!< Task priorities
#define ZIGBEE_TASK_PRIORITY             3        //!< Task priorities
#else
#define THREAD_TASK_PRIORITY             4        //!< Task priorities
#define ZIGBEE_TASK_PRIORITY             4        //!< Task priorities
#endif
#define THREAD_TASK_STACK_SIZE           512 * 8  //!<  Task stack size
#define ZIGBEE_TASK_STACK_SIZE           512 * 8  //!<  Task stack size
#define MATTER_TASK_PRIORITY             1        //!< Task priorities
#define MATTER_TASK_STACK_SIZE           512 * 8  //!<  Task stack size

#if defined (__cplusplus)
}
#endif

#endif /* ! ZB_TST_H_ */
