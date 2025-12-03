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
#include <os_sched.h>
#include <string.h>
#include <stdlib.h>
#include <trace.h>
#include <os_task.h>
#include <os_sync.h>
#include "rtl_pinmux.h"
#include "rtl_rcc.h"
#include "rtl_gpio.h"
#include "rtl_uart.h"
#include "rtl_tim.h"
#include "rtl_nvic.h"

#include "zb_tst_cfg.h"
#include "vector_table_ext.h"

#include "app_section.h"
#include "mac_driver.h"
#include "mac_driver_mpan.h"
#include "platform-bee.h"
#include "pm.h"

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
void *zb_task_handle;   //!< ZB MAC Task handle
extern void shell_cmd_init(void);
extern void otPlatFlashErase(uint32_t *, uint8_t);
extern int32_t matter_kvs_clean(void);

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
void zb_pin_mux_init(void)
{
#if (1 == BUILD_RCP)
#ifdef BOARD_RTL8771GTV
    // UART for CLI
    Pad_Config(config_param.tx_pin, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
               PAD_OUT_HIGH);
    Pad_Config(config_param.rx_pin, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
               PAD_OUT_HIGH);
    if (((config_param.func_msk & (1 << HW_FLOWCTL_OFFSET)) == 0x00) ||
        ((config_param.func_msk & (1 << FORCE_HW_FLOWCTL_OFFSET)) == 0x00))
    {
        Pad_Config(config_param.rts_pin, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
                   PAD_OUT_LOW);
        Pad_Config(config_param.cts_pin, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
                   PAD_OUT_LOW);
    }

    Pinmux_Config(config_param.tx_pin, ZB_CLI_UART_TX);
    Pinmux_Config(config_param.rx_pin, ZB_CLI_UART_RX);
    if (((config_param.func_msk & (1 << HW_FLOWCTL_OFFSET)) == 0x00) ||
        ((config_param.func_msk & (1 << FORCE_HW_FLOWCTL_OFFSET)) == 0x00))
    {
        Pinmux_Config(config_param.rts_pin, ZB_CLI_UART_RTS);
        Pinmux_Config(config_param.cts_pin, ZB_CLI_UART_CTS);
    }
#endif

    // External PA
    if (config_param.ext_pa != 0xff)
    {
        if (config_param.ext_pa_lna_ploatiry & (1 << 0))
        {
            Pad_Config(config_param.ext_pa, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
                       PAD_OUT_HIGH);
        }
        else
        {
            Pad_Config(config_param.ext_pa, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
                       PAD_OUT_LOW);
        }
    }

    // External LNA
    if (config_param.ext_lna != 0xff)
    {
        if (config_param.ext_pa_lna_ploatiry & (1 << 1))
        {
            Pad_Config(config_param.ext_lna, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
                       PAD_OUT_HIGH);
        }
        else
        {
            Pad_Config(config_param.ext_lna, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
                       PAD_OUT_LOW);
        }
    }
#else
    // UART for CLI
    Pad_Config(ZB_CLI_UART_TX_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
               PAD_OUT_HIGH);
    Pad_Config(ZB_CLI_UART_RX_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE,
               PAD_OUT_HIGH);
    Pinmux_Config(ZB_CLI_UART_TX_PIN, ZB_CLI_UART_TX);
    Pinmux_Config(ZB_CLI_UART_RX_PIN, ZB_CLI_UART_RX);
#endif
}

POWER_CheckResult IO_UART_DLPS_Enter_Allowed = POWER_CHECK_PASS;
POWER_CheckResult io_uart_dlps_check(void)
{
    return IO_UART_DLPS_Enter_Allowed;
}

void io_uart_dlps_allow_enter(void)
{
    IO_UART_DLPS_Enter_Allowed = POWER_CHECK_PASS;
}

void io_uart_dlps_disallow_enter(void)
{
    IO_UART_DLPS_Enter_Allowed = POWER_CHECK_FAIL;
}

extern volatile uint32_t app_wakeup_reason;
void io_uart_system_wakeup(void)
{
    if (System_WakeUpInterruptValue(ZB_CLI_UART_RX_PIN) == SET)
    {
        Pad_ClearWakeupINTPendingBit(ZB_CLI_UART_RX_PIN);
        System_WakeUpPinDisable(ZB_CLI_UART_RX_PIN);
        IO_UART_DLPS_Enter_Allowed = POWER_CHECK_FAIL;
        app_wakeup_reason = APP_WAKEUP_REASON_UART_RX;
    }
}

void io_uart_dlps_enter(void)
{
    /* Notes: DBG_DIRECT is only used in debug demo, do not use in app project.*/
    //DBG_DIRECT("DLPS ENTER");
    /* Switch pad to Software mode */
    Pad_ControlSelectValue(ZB_CLI_UART_TX_PIN, PAD_SW_MODE);
    Pad_ControlSelectValue(ZB_CLI_UART_RX_PIN, PAD_SW_MODE);

    System_WakeUpPinEnable(ZB_CLI_UART_RX_PIN, PAD_WAKEUP_POL_LOW, PAD_WAKEUP_DEB_DISABLE);
}

void io_uart_dlps_exit(void)
{
    /* Notes: DBG_DIRECT is only used in debug demo, do not use in app project.*/
    //DBG_DIRECT("DLPS EXIT");
    Pad_ControlSelectValue(ZB_CLI_UART_TX_PIN, PAD_PINMUX_MODE);
    Pad_ControlSelectValue(ZB_CLI_UART_RX_PIN, PAD_PINMUX_MODE);
}

/**
 * @brief    Contains the initialization of peripherals
 * @note     Both new architecture driver and legacy driver initialization method can be used
 * @return   void
 */
void zb_periheral_drv_init(void)
{
#if (1 == BUILD_RCP)
    DBG_DIRECT("uart_tx_pin %d", config_param.tx_pin);
    DBG_DIRECT("uart_rx_pin %d", config_param.rx_pin);
    DBG_DIRECT("uart_rts_pin %d", config_param.rts_pin);
    DBG_DIRECT("uart_cts_pin %d", config_param.cts_pin);
    DBG_DIRECT("uart_func_msk %x", config_param.func_msk);
    DBG_DIRECT("uart_baudrate %d", config_param.baud_rate);;
    DBG_DIRECT("pta_dis %x", config_param.pta_dis);
    DBG_DIRECT("pta_wl_act %d", config_param.pta_wl_act);
    DBG_DIRECT("pta_bt_act %d", config_param.pta_bt_act);
    DBG_DIRECT("pta_bt_stat %d", config_param.pta_bt_stat);
    DBG_DIRECT("pta_bt_clk %d", config_param.pta_bt_clk);
    DBG_DIRECT("ext_pa %d", config_param.ext_pa);
    DBG_DIRECT("ext_lna %d", config_param.ext_lna);
    DBG_DIRECT("ext_pa_lna_polarity %x", config_param.ext_pa_lna_ploatiry);
#endif
}

extern void Zigbee_Handler_Patch(void);

void zb_mac_interrupt_enable(void)
{
    //NVIC_InitTypeDef NVIC_InitStruct;
    // TODO: enable MAC interrupt
    /* share the same IRQ number with BT_MAC on FPGA temporary, so the interrupt
       shall be initialed in BT lower stack initialization */
    NVIC_SetPriority(Zigbee_IRQn, 2);
    NVIC_EnableIRQ(Zigbee_IRQn);
    RamVectorTableUpdate_ext(Zigbee_VECTORn, Zigbee_Handler_Patch);
}

int32_t edscan_level2dbm(int32_t level)
{
    return (level * 2) - 90;
}

extern uint32_t (*lowerstack_SystemCall)(uint32_t opcode, uint32_t param, uint32_t param1,
                                         uint32_t param2);
extern void set_zigbee_priority(uint16_t priority, int16_t priority_min);
extern uint32_t get_zigbee_window_slot_imp(int16_t *prio, int16_t *prio_min);
extern void (*modem_set_zb_cca_combination)(uint8_t comb);

mac_attribute_t attr;
mac_driver_t drv;
pan_mac_comm_t pan_mac_comm;
pan_mac_t pan_mac[MAX_PAN_NUM];

extern void mac_SetTxNCsma(bool enable);
void zb_mac_drv_init(void)
{
    mac_InitAttribute(&attr);
    attr.mac_cfg.rf_early_term = 0;
    //attr.mac_cfg.frm06_rx_early = 0;
#if (BUILD_RCP == 1)
    attr.phy_arbitration_en = 0;
#else
    attr.mac_cfg.anch_jump_val = 3;
    lowerstack_SystemCall(10, 1, 512, -1);
#endif
    mac_Enable();
    mac_Initialize(&drv, &attr);
    mac_Initialize_Additional();
}

typedef void (*bt_hci_reset_handler_t)(void);
extern void mac_RegisterBtHciResetHanlder(bt_hci_reset_handler_t handler);
void zb_mac_drv_enable(void)
{
    int8_t tx_power_default;
    mpan_CommonInit(&pan_mac_comm);
    mpan_Init(&pan_mac[0], MPAN_PAN0);
    zb_mac_drv_init();
    mac_RegisterBtHciResetHanlder(zb_mac_drv_init);
    mac_RegisterCallback(NULL, edscan_level2dbm, set_zigbee_priority,
                         *modem_set_zb_cca_combination);
    mac_SetCcaMode(MAC_CCA_CS_ED);
    tx_power_default = mac_GetTXPower_patch();
    mac_SetTXPower_patch(tx_power_default);
}

extern void mac_Initialize_Patch(void);
extern void startup_task_init(void);

void zb_task_init(void)
{
#if (1 == BUILD_RCP)
    extern void mac_LoadConfigParam(void);
    mac_LoadConfigParam();
#endif

    mac_Initialize_Patch();

    zb_pin_mux_init();
    zb_periheral_drv_init();
    zb_mac_interrupt_enable();
    zb_mac_drv_enable();

    startup_task_init();
}

/** @} */ /* End of group PERIPH_DEMO_MAIN */

#define CHECK_STR_UNALIGNED(X, Y) \
    (((uint32_t)(X) & (sizeof (uint32_t) - 1)) | \
     ((uint32_t)(Y) & (sizeof (uint32_t) - 1)))

#define STR_OPT_BIGBLOCKSIZE     (sizeof(uint32_t) << 2)

#define STR_OPT_LITTLEBLOCKSIZE (sizeof (uint32_t))

APP_FLASH_TEXT_SECTION void *__wrap_memcpy(void *s1, const void *s2, size_t n)
{
    char *dst = (char *) s1;
    const char *src = (const char *) s2;

    uint32_t *aligned_dst;
    const uint32_t *aligned_src;

    /* If the size is small, or either SRC or DST is unaligned,
     * then punt into the byte copy loop.  This should be rare.
     */
    if (n < sizeof(uint32_t) || CHECK_STR_UNALIGNED(src, dst))
    {
        while (n--)
        {
            *dst++ = *src++;
        }

        return s1;
    } /* if */

    aligned_dst = (uint32_t *)dst;
    aligned_src = (const uint32_t *)src;

    /* Copy 4X long words at a time if possible.  */
    while (n >= STR_OPT_BIGBLOCKSIZE)
    {
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        n -= STR_OPT_BIGBLOCKSIZE;
    } /* while */

    /* Copy one long word at a time if possible.  */
    while (n >= STR_OPT_LITTLEBLOCKSIZE)
    {
        *aligned_dst++ = *aligned_src++;
        n -= STR_OPT_LITTLEBLOCKSIZE;
    } /* while */

    /* Pick up any residual with a byte copier.  */
    dst = (char *)aligned_dst;
    src = (const char *)aligned_src;
    while (n--)
    {
        *dst++ = *src++;
    }

    return s1;
} /* _memcpy() */

#define wsize   sizeof(uint32_t)
#define wmask   (wsize - 1)

void *__wrap_memset(void *dst0, int Val, size_t length)
{
    size_t t;
    uint32_t Wideval;
    uint8_t *dst;

    dst = dst0;
    /*
     * If not enough words, just fill bytes.  A length >= 2 words
     * guarantees that at least one of them is `complete' after
     * any necessary alignment.  For instance:
     *
     *  |-----------|-----------|-----------|
     *  |00|01|02|03|04|05|06|07|08|09|0A|00|
     *            ^---------------------^
     *       dst         dst+length-1
     *
     * but we use a minimum of 3 here since the overhead of the code
     * to do word writes is substantial.
     */
    if (length < 3 * wsize)
    {
        while (length != 0)
        {
            *dst++ = Val;
            --length;
        }
        return (dst0);
    }

    if ((Wideval = (uint32_t)Val) != 0)     /* Fill the word. */
    {
        Wideval = ((Wideval << 24) | (Wideval << 16) | (Wideval << 8) | Wideval); /* u_int is 32 bits. */
    }

    /* Align destination by filling in bytes. */
    if ((t = (uint32_t)dst & wmask) != 0)
    {
        t = wsize - t;
        length -= t;
        do
        {
            *dst++ = Val;
        }
        while (--t != 0);
    }

    /* Fill words.  Length was >= 2*words so we know t >= 1 here. */
    t = length / wsize;
    do
    {
        *(uint32_t *)dst = Wideval;
        dst += wsize;
    }
    while (--t != 0);

    /* Mop up trailing bytes, if any. */
    t = length & wmask;
    if (t != 0)
        do
        {
            *dst++ = Val;
        }
        while (--t != 0);

    return (dst0);
}

#include <string.h>
#include "os_mem.h"

APP_FLASH_TEXT_SECTION void *__wrap__malloc_r(struct _reent *ptr, size_t size)
{
    void *mem;
    mem = os_mem_alloc(RAM_TYPE_DATA_ON, size);
    return mem;
}

APP_FLASH_TEXT_SECTION void __wrap__free_r(struct _reent *ptr, void *addr)
{
    os_mem_free(addr);
}

/**
 * @warning This realloc implementation has critical limitations due to os_mem API constraints.
 *
 * PROBLEM:
 *   The underlying os_mem_alloc API does not provide a way to retrieve the original
 *   allocation size. Standard realloc should copy min(oldsize, newsize) bytes, but
 *   oldsize is unknown.
 *
 * CURRENT USAGE STATUS (as of analysis):
 *   - OpenThread core: Does NOT use realloc (only uses Heap::CAlloc/Free)
 *   - mbedtls: Implements its own safe resize_buffer() that correctly handles sizes
 *   - Result: This function should never be called in practice
 *
 * IMPLEMENTATION DECISION:
 *   This implementation returns NULL (allocation failure) for resize attempts to
 *   prevent potential buffer over-read vulnerabilities. This is safer than:
 *   - Copying newsize bytes (buffer over-read if newsize > oldsize)
 *   - Copying arbitrary amount (data loss if amount < min(oldsize, newsize))
 *
 * RATIONALE:
 *   Explicit failure is better than silent memory corruption or undefined behavior.
 *   If this causes issues, the caller should implement size tracking like mbedtls does.
 *
 * @param ptr     Reentrant structure (unused)
 * @param mem     Pointer to previously allocated memory, or NULL
 * @param newsize New size in bytes, or 0 to free
 * @return        Pointer to allocated memory, or NULL on failure
 */
APP_FLASH_TEXT_SECTION void *__wrap__realloc_r(struct _reent *ptr, void *mem, size_t newsize)
{
    /* realloc(ptr, 0) is equivalent to free(ptr) */
    if (!newsize)
    {
        if (mem)
        {
            os_mem_free(mem);
        }
        return NULL;
    }

    /* realloc(NULL, size) is equivalent to malloc(size) */
    if (!mem)
    {
        return os_mem_alloc(RAM_TYPE_DATA_ON, newsize);
    }

    /*
     * For mem != NULL && newsize != 0:
     * Cannot safely resize existing allocation without knowing original size.
     * Return NULL to indicate failure rather than risk memory corruption.
     *
     * If this causes issues, the caller should either:
     * 1. Track allocation sizes internally (like mbedtls resize_buffer does)
     * 2. Use explicit alloc+copy+free with known sizes
     * 3. Avoid realloc and use fixed-size allocations
     */
    return NULL;
}

APP_FLASH_TEXT_SECTION void *__wrap__calloc_r(struct _reent *ptr, size_t size, size_t len)
{
    void *mem;
    mem = os_mem_zalloc(RAM_TYPE_DATA_ON, (size * len));
    return mem;
}
