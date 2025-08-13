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

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <stdlib.h>
#include <os_sched.h>
#include <trace.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_bond_le.h>
#include <profile_server.h>
#include <gap_msg.h>
#if F_BT_ANCS_CLIENT_SUPPORT
#include <profile_client.h>
#include <ancs.h>
#endif
#include "board.h"
#include "mem_config.h"
#include "pm.h"
#include "rtl_nvic.h"
#include "io_dlps.h"
#include "app_section.h"
#ifdef BUILD_MATTER
#include "matter_gpio.h"
#endif
#if FEATURE_SUPPORT_CFU
#include "cfu_application.h"
#endif
#include "flash_map.h"
#include "ftl.h"
#if FEATURE_TRUSTZONE_ENABLE
#include "nsc_veneer_customize.h"
#endif

POWER_CheckResult dlps_allow = POWER_CHECK_PASS;

/** @defgroup  PERIPH_DEMO_MAIN Peripheral Main
    * @brief Main file to initialize hardware and BT stack and start task scheduling
    * @{
    */

/*============================================================================*
 *                              Constants
 *============================================================================*/

/*============================================================================*
 *                              Variables
 *============================================================================*/

/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief    Contains the initialization of pinmux settings and pad settings
 * @note     All the pinmux settings and pad settings shall be initiated in this function,
 *           but if legacy driver is used, the initialization of pinmux setting and pad setting
 *           should be peformed with the IO initializing.
 * @return   void
 */
void board_init(void)
{

}

/**
 * @brief    Contains the initialization of peripherals
 * @note     Both new architecture driver and legacy driver initialization method can be used
 * @return   void
 */
void driver_init(void)
{

}

/**
 * @brief    System_Handler
 * @note     system handle to judge which pin is wake source
 * @return   void
 */
volatile uint32_t app_wakeup_reason = APP_WAKEUP_REASON_NONE;
extern void io_uart_system_wakeup(void);
RAM_FUNCTION
void System_Handler(void)
{
#if DLPS_EN
#if defined(BOARD_RTL8777G)
#if defined(BUILD_MATTER)
    if (System_WakeUpInterruptValue(BUTTON_SW1) == SET)
    {
        //DBG_DIRECT("SW1 Wake up");
        Pad_ClearWakeupINTPendingBit(BUTTON_SW1);
        System_WakeUpPinDisable(BUTTON_SW1);
        matter_gpio_disallow_to_enter_dlps();
        app_wakeup_reason = APP_WAKEUP_REASON_BUTTON_SW1;
    }

    if (System_WakeUpInterruptValue(BUTTON_SW2) == SET)
    {
        //DBG_DIRECT("SW2 Wake up");
        Pad_ClearWakeupINTPendingBit(BUTTON_SW2);
        System_WakeUpPinDisable(BUTTON_SW2);
        matter_gpio_disallow_to_enter_dlps();
        app_wakeup_reason = APP_WAKEUP_REASON_BUTTON_SW2;
    }

    if (System_WakeUpInterruptValue(BUTTON_SW3) == SET)
    {
        //DBG_DIRECT("SW3 Wake up");
        Pad_ClearWakeupINTPendingBit(BUTTON_SW3);
        System_WakeUpPinDisable(BUTTON_SW3);
        matter_gpio_disallow_to_enter_dlps();
        app_wakeup_reason = APP_WAKEUP_REASON_BUTTON_SW3;
    }

    if (System_WakeUpInterruptValue(BUTTON_SW4) == SET)
    {
        //DBG_DIRECT("SW4 Wake up");
        Pad_ClearWakeupINTPendingBit(BUTTON_SW4);
        System_WakeUpPinDisable(BUTTON_SW4);
        matter_gpio_disallow_to_enter_dlps();
        app_wakeup_reason = APP_WAKEUP_REASON_BUTTON_SW4;
    }
#endif
#if (ENABLE_CLI == 1)
    io_uart_system_wakeup();
#endif
#endif
#endif
}

/**
 * @brief this function will be called before enter DLPS
 *
 *  set PAD and wakeup pin config for enterring DLPS
 *
 * @param none
 * @return none
 * @retval void
*/
extern void io_uart_dlps_enter(void);

/**
 * @brief this function will be called after exit DLPS
 *
 *  set PAD and wakeup pin config for enterring DLPS
 *
 * @param none
 * @return none
 * @retval void
*/
extern void io_uart_dlps_exit(void);

void app_dlps_enter(void)
{
#if defined(BOARD_RTL8777G)
#if defined(BUILD_MATTER)
    matter_gpio_dlps_enter();
#endif
#if (ENABLE_CLI == 1)
    io_uart_dlps_enter();
#endif
#endif
}

extern bool zbmac_pm_check_inactive(void);
extern void zbmac_pm_initiate_wakeup(void);
void app_dlps_exit(void)
{
#if defined(BOARD_RTL8777G)
#if defined(BUILD_MATTER)
    matter_gpio_dlps_exit();
#endif
#if (ENABLE_CLI == 1)
    io_uart_dlps_exit();
#endif
#endif
    if (zbmac_pm_check_inactive())
    {
        zbmac_pm_initiate_wakeup();
    }
}

/**
 * @brief DLPS CallBack function
 * @param none
* @return true : allow enter dlps
 * @retval void
*/
extern POWER_CheckResult io_uart_dlps_check(void);
RAM_FUNCTION
POWER_CheckResult app_dlps_check_cb(void)
{
    POWER_CheckResult ret = dlps_allow;

#if defined(BOARD_RTL8777G)
#if defined(BUILD_MATTER)
    ret = matter_gpio_dlps_check();
#endif
#if (ENABLE_CLI == 1)
    ret = io_uart_dlps_check();
#endif
#endif

    return ret;
}

/**
 * @brief    Contains the power mode settings
 * @return   void
 */
void pwr_mgr_init(void)
{
#if DLPS_EN
    power_check_cb_register(app_dlps_check_cb);
    DLPS_IORegUserDlpsEnterCb(app_dlps_enter);
    DLPS_IORegUserDlpsExitCb(app_dlps_exit);
    DLPS_IORegister();
    bt_power_mode_set(BTPOWER_DEEP_SLEEP);
    power_mode_set(POWER_DLPS_MODE);
#endif
}

void system_clock_init(void)
{
#if defined(DLPS_EN) && (DLPS_EN == 1)
    int32_t ret;
    ret = flash_nor_try_high_speed_mode(0, FLASH_NOR_4_BIT_MODE);
    APP_PRINT_INFO1("ret %d", ret);
#else
    uint32_t actual_mhz;
    int32_t ret0, ret1, ret2, ret3;
    ret0 = pm_cpu_freq_set(125, &actual_mhz);
    ret1 = flash_nor_set_seq_trans_enable(FLASH_NOR_IDX_SPIC0, 1);
    ret2 = fmc_flash_nor_clock_switch(FLASH_NOR_IDX_SPIC0, 160, &actual_mhz);

    ret3 = flash_nor_try_high_speed_mode(0, FLASH_NOR_4_BIT_MODE);
    APP_PRINT_INFO5("ret0 %d , ret1 %d , ret2 %d , ret3 %d, actual_mhz %d", ret0, ret1, ret2, ret3,
                    actual_mhz);
#endif
}

extern int xmodemReceive(unsigned char *dest, int destsz);
extern void zb_task_init(void);
/**
 * @brief    Entry of APP code
 * @return   int (To avoid compile warning)
 */
int rtk_main(void)
{
#if FEATURE_TRUSTZONE_ENABLE
    uint32_t param = 0;
    secure_app_function_call(SECURE_APP_FUNCTION_TEST, &param);
    DBG_DIRECT("param %d", param);
#endif

    if (FEATURE_TRUSTZONE_ENABLE)
    {
        DBG_DIRECT("Non-Secure World: main");
    }
    else
    {
        DBG_DIRECT("Secure World: main");
    }

    system_clock_init();

    extern uint32_t random_seed_value;
    srand(random_seed_value);

    board_init();
    pwr_mgr_init();

#if ((ENABLE_CLI == 1 && XMODEM_ENABLE == 1) || FEATURE_SUPPORT_CFU)
    ftl_init_module("app", 0x3F8, 4);
#endif

#if (FEATURE_SUPPORT_CFU && (CFU_MODE == NORMAL_MODE))
    is_app_in_cfu_mode = false;
    load_enter_cfu_mode_flag();
    if (ALLOW_TO_ENTER_CFU_MODE_FLAG == get_enter_cfu_mode_flag())
    {
        APP_PRINT_INFO0("[rtk_main] in cfu mode");
        init_cfu_mode();
    }
    else
    {
        zb_task_init();
    }
#else

#if (ENABLE_CLI == 1 && XMODEM_ENABLE == 1)
    int32_t ftl_res = 0;
    uint32_t boot_flag = 0;

    ftl_res = ftl_load_from_module("app", &boot_flag, 0, sizeof(boot_flag));
    if (0 != ftl_res)
    {
        ftl_save_to_module("app", &boot_flag, 0, sizeof(boot_flag));
        ftl_load_from_module("app", &boot_flag, 0, sizeof(boot_flag));
    }

    if (0xdeadbeef == boot_flag)
    {
        xmodemReceive(NULL, OTA_TMP_SIZE);
    }
    else
#endif
    {
        zb_task_init();
    }
#endif
    os_sched_start();

    return 0;
}
/** @} */ /* End of group PERIPH_DEMO_MAIN */


