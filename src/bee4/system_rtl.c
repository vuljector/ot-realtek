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

#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include "app_section.h"
#include "debug_monitor.h"
#include "rtl876x.h"
#include "rtl876x_lib_platform.h"
#include "rom_uuid.h"
#include "patch_header_check.h"
#include "system_rtl876x.h"
#include "vector_table_ext.h"
#include "vector_table.h"
#include "os_mem.h"
#include "mem_config.h"
#include "os_sched.h"
#include "os_pm.h"
#include "trace.h"
#include "utils.h"
#include "flash_nor_device.h"
#include "version.h"
#include "aon_reg.h"
#include "wdt.h"
#include "rtl_gpio.h"
#include "otp.h"
#include "otp_config.h"
#include "ftl.h"
#include "fmc_api.h"
#include "clock.h"
#if (SUPPORT_NORMAL_OTA == 1)
#include "dfu_common.h"
#include "dfu_main.h"
#endif
#if (USE_PSRAM == 1)
#include "psram.h"
#endif

#define BOOT_DIRECT_DEBUG        1
#define DEBUG_WATCHPOINT_ENABLE  0

/*os config*/
#define USE_ROM_OS      0
#define USE_FREERTOS    1
#define USE_RTT         2

#define FreeRTOS_portBYTE_ALIGNMENT    8

typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK *pxNextFreeBlock;   /*<< The next free block in the list. */
    size_t xBlockSize : 28;                 /*<< The size of the free block. */
    size_t xRamType : 3;
    size_t xAllocateBit : 1;
} BlockLink_t;

bool system_init(void) APP_FLASH_TEXT_SECTION;

#if (ENCRYPT_RAM_CODE == 1)
extern char Image$$RAM_TEXT$$RO$$Base[];
extern char Load$$RAM_TEXT$$RO$$Base[];
extern char Load$$RAM_TEXT$$RO$$Length[];
#else
extern char Image$$FLASH_TEXT$$RO$$Base[];
extern char Load$$FLASH_TEXT$$RO$$Base[];
extern char Load$$FLASH_TEXT$$RO$$Length[];
#endif
#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
extern char Load$$LR$$LOAD_FLASH$$Base[];
#if (FEATURE_FLASH_SEC==1)
extern unsigned int Load$$LR$$LOAD_FLASH$$Length;
#endif
#elif defined (__GNUC__)
#if (FEATURE_FLASH_SEC==1)
extern uint32_t *__flash_sec_base_addr__;
extern uint32_t *__flash_sec_region_size__;
#endif
extern uint32_t *__app_flash_load_ad__;
#endif

BOOL_WDT_CB user_wdt_cb __attribute__((weak)) = NULL;
USER_CALL_BACK app_pre_main_cb __attribute__((weak)) = NULL ;
USER_CALL_BACK os_patch_init __attribute__((weak)) = NULL ;

#if defined (__GNUC__)
uint32_t _init_array __attribute__((weak));
extern uint32_t _einit_array __attribute__((alias("_init_array")));
#endif

T_SW_RESET_REASON sw_reset_reason;
T_WDT_RESET_TYPE  wdt_reset_type;

uint32_t random_seed_value;
void random_seed_init(void);
bool Reset_Handler(void);

const T_IMG_HEADER_FORMAT img_header APP_FLASH_HEADER =
{
    .auth =
    {
        .image_mac = {[0 ... 15] = 0xFF},
    },
    .ctrl_header =
    {
        .ic_type = IMG_IC_TYPE,
        .secure_version = 0,
#if (ENCRYPT_RAM_CODE == 1)
        .ctrl_flag.xip = 0,
#if (FEATURE_ENCRYPTION == 1)
        .ctrl_flag.enc = 1,
#else
        .ctrl_flag.enc = 0,
#endif
        .ctrl_flag.load_when_boot = 1,
        .ctrl_flag.enc_load = 0,
        .ctrl_flag.enc_key_select = ENC_KEY_OCEK,
#else
        .ctrl_flag.xip = 1,
        .ctrl_flag.enc = 0,
        .ctrl_flag.load_when_boot = 0,
        .ctrl_flag.enc_load = 0,
        .ctrl_flag.enc_key_select = 0,
#endif
        .ctrl_flag.not_obsolete = 1,
        .image_id = IMG_MCUAPP,
        .payload_len = 0x100,    //Will modify by build tool later
    },
    .uuid = DEFINE_symboltable_uuid,

#if (ENCRYPT_RAM_CODE == 1)
    /* to be modified based on different user scenario */
    .load_src = (uint32_t)Load$$RAM_TEXT$$RO$$Base,
    .load_dst = (uint32_t)Image$$RAM_TEXT$$RO$$Base,
    .load_len = (uint32_t)Load$$RAM_TEXT$$RO$$Length,
    .exe_base = (uint32_t)Image$$RAM_TEXT$$RO$$Base,
#else
    /* XIP test */
#if defined (__ARMCC_VERSION)
    .load_src = (uint32_t)Load$$FLASH_TEXT$$RO$$Base,
    .load_dst = (uint32_t)Image$$FLASH_TEXT$$RO$$Base,
    .load_len = 0,  //0 indicates all XIP
    .exe_base = (uint32_t)Image$$FLASH_TEXT$$RO$$Base,
#elif defined (__GNUC__)
    .load_src = (uint32_t)Reset_Handler,
    .load_dst = (uint32_t)Reset_Handler,
    .load_len = 0,  //0 indicates all XIP
    .exe_base = (uint32_t)Reset_Handler,
#endif
#endif
#if defined (__ARMCC_VERSION)
    .image_base = (uint32_t) &Load$$LR$$LOAD_FLASH$$Base,
#elif defined (__GNUC__)
    .image_base = (uint32_t) &__app_flash_load_ad__,
#endif
    .git_ver =
    {
        .ver_info.sub_version._version_major = VERSION_MAJOR,
        .ver_info.sub_version._version_minor = VERSION_MINOR,
        .ver_info.sub_version._version_revision = VERSION_REVISION,
        .ver_info.sub_version._version_reserve = VERSION_BUILDNUM % 32, //only 5 bit
        ._version_commitid = VERSION_GCID,
        ._customer_name = {CN_1, CN_2, CN_3, CN_4, CN_5, CN_6, CN_7, CN_8},
    },
#if (FEATURE_FLASH_SEC == 1)
    .flash_sec_cfg =
    {
        .ctrl_flag.enable = 1,
        .ctrl_flag.mode = 1,
        .ctrl_flag.key_select = ENC_KEY_OCEK_WITH_OEMCONST,
#if defined (__ARMCC_VERSION)
        .base_addr = (uint32_t) Load$$FLASH_TEXT$$RO$$Base,
        .region_size = (uint32_t) &Load$$LR$$LOAD_FLASH$$Length - 4096,
#elif defined (__GNUC__)
        .base_addr = (uint32_t) &__flash_sec_base_addr__,
        .region_size = (uint32_t) &__flash_sec_region_size__,
#endif
        .iv_high = {0x12, 0x34, 0x56, 0x78},
        .iv_low = {0x11, 0x22, 0x33, 0x44},
    },
#endif

    .ex_info.app_ram_info =
    {
        .app_ram_data_addr = NS_RAM_APP_ADDR,
        .app_ram_data_size = NS_RAM_APP_SIZE,
        .app_heap_data_on_size = NS_HEAP_SIZE,
        .ext_data_sram_size = EXT_DATA_SRAM_GLOBAL_SIZE,
    }
};


__WEAK void print_sdk_lib_version(void)
{
}

#if (DEBUG_WATCHPOINT_ENABLE == 1)
/**
 * @brief  Enable Debug Monitor Function (include NVIC Enable and DWT configuration)
 * @param  none
 * @return none
 */
void debug_monitor_enable(void)
{

    //set debug monitor priority
    NVIC_SetPriority(DebugMonitor_IRQn, 3);

    if ((CoreDebug->DHCSR & 0x1) != 0)
    {
        DBG_DIRECT("- Halting Debug Enabled - Can't Enable Monitor Mode Debug! -");
        return;
    }
    enable_debug_monitor();

    //watch_point_0_setting(0x00141d08, WORD, DEBUG_EVENT, FUNCTION_DADDR_W);
    //watch_point_1_setting(0x00450004, WORD, DEBUG_EVENT, FUNCTION_DADDR_W);

    return;
}
#endif

void common_main(void)
{
    uint32_t cpu_clk = 0;
    pm_cpu_freq_get(&cpu_clk);
#if (BOOT_DIRECT_DEBUG == 1)
    if (FEATURE_TRUSTZONE_ENABLE)
    {
        DBG_DIRECT("Non-Secure World: common_main in app, Timestamp: %d us", read_cpu_counter() / cpu_clk);
    }
    else
    {
        DBG_DIRECT("Secure World: common_main in app, Timestamp: %d us", read_cpu_counter() / cpu_clk);
    }
#endif

//add common system code here before enter user defined main function

    // add xip vector wrapper for btmac handler, which has been updated in lowerstack init
#if (FEATURE_TRUSTZONE_ENABLE == 1)
    IRQ_Fun *pRamVector    = (IRQ_Fun *)NS_RAM_VECTOR_ADDR;
#else
    IRQ_Fun *pRamVector    = (IRQ_Fun *)S_RAM_VECTOR_ADDR;
#endif
    RamVectorTableUpdate_ext(BTMAC_VECTORn, (IRQ_Fun)pRamVector[BTMAC_VECTORn]);

    print_sdk_lib_version();

#if (SYSTEM_TRACE_ENABLE == 1)
    extern void system_trace_init(void);
    system_trace_init();

    extern void print_system_trace_lib_version(void);
    print_system_trace_lib_version();
#endif

    /*config bt stack parameters in rom*/
#ifdef BT_STACK_CONFIG_ENABLE
    bt_stack_config_init();
#endif

#if (FEATURE_TRUSTZONE_ENABLE == 1)
    /*target IRQs to non-secure*/
    setup_non_secure_nvic();
#endif

    random_seed_init();

    patch_func_ptr_init();

    if (!OTP->ftl_init_in_rom && OTP->use_ftl)
    {
        extern void (*flash_task_init)(void);
        flash_task_init();

#if (FTL_POOL == 1)
        extern int32_t ftl_pool_init(uint32_t ftl_pool_startAddr, uint32_t ftl_pool_size,
                                     uint16_t default_module_logicSize, uint8_t gc_page_thres);
        uint16_t rom_ftl_size = 0xC00;
        uint8_t ret = 0;
        uint8_t thres = 2; // default thre val

#ifdef FTL_GC_PAGE_THRES_CUSTOMIZED
        thres = FTL_GC_PAGE_THRES_CUSTOMIZED
#endif
        ret = ftl_pool_init(flash_nor_get_bank_addr(FLASH_FTL),
                            flash_nor_get_bank_size(FLASH_FTL),
                            rom_ftl_size, thres);
        FLASH_PRINT_INFO1("[FTL] init ret=%d", ret);
#else
        extern uint32_t (*ftl_init)(uint32_t u32PageStartAddr, uint8_t pagenum);
        uint8_t ret = 0;
        ret = ftl_init(flash_nor_get_bank_addr(FLASH_FTL),
                       flash_nor_get_bank_size(FLASH_FTL) / 0x1000);
        FLASH_PRINT_INFO1("[FTL] init ret=%d", ret);
#endif
    }

#if (SUPPORT_NORMAL_OTA == 1)
    if (dfu_check_ota_mode_flag())
    {
        DBG_DIRECT("DFU TASK");
        dfu_main();
        dfu_set_ota_mode_flag(false);

        os_sched_start();
    }
#endif

#if defined ( __ICCARM__ )
    extern void __iar_program_start(const int);
    __iar_program_start(0);
#elif defined (__ARMCC_VERSION)
    extern int __main(void);
    __main();
#elif defined (__GNUC__)
    typedef void (*init_fn_t)(void);
    init_fn_t *fp;
    // cppcheck-suppress comparePointers
    for (fp = (init_fn_t *)&_init_array; fp < (init_fn_t *)&_einit_array; fp++)
    {
        (*fp)();
    }
#if defined (CONFIG_RTK_MAIN_FUN)
    extern int rtk_main(void);
    rtk_main();
#else
    extern int main(void);
    main();
#endif
#endif
}

void random_seed_init(void)
{
    random_seed_value = platform_random(0xFFFFFFFF);
}

void ram_init(void) APP_FLASH_TEXT_SECTION;
void ram_init(void)
{
    unsigned int *section_image_base = 0;
    unsigned int *section_load_base = 0;
    unsigned int section_image_length = 0;
    unsigned int i = 0;

    //copy dtcm ro
#if defined (__ARMCC_VERSION)
    extern unsigned int Image$$ER_IRAM_NS$$RO$$Base;
    extern unsigned int Load$$ER_IRAM_NS$$RO$$Base;
    extern unsigned int Image$$ER_IRAM_NS$$RO$$Length;

    section_image_base = (unsigned int *)& Image$$ER_IRAM_NS$$RO$$Base;
    section_load_base = (unsigned int *)& Load$$ER_IRAM_NS$$RO$$Base;
    section_image_length = (unsigned int)& Image$$ER_IRAM_NS$$RO$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__enter_iram_ns_ro_exe_ad__;
    extern unsigned int *__enter_iram_ns_ro_load_ad__;
    extern unsigned int *__enter_iram_ns_ro_length__;

    section_image_base = (unsigned int *)& __enter_iram_ns_ro_exe_ad__;
    section_load_base = (unsigned int *)& __enter_iram_ns_ro_load_ad__;
    section_image_length = (unsigned int)& __enter_iram_ns_ro_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memcpy(section_image_base, section_load_base, section_image_length);
#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("APP copy DTCM RO: src 0x%x, dest 0x%x, len %d",
               (uint32_t)section_load_base, (uint32_t)section_image_base, section_image_length);
#endif

    //copy dtcm rw
#if defined (__ARMCC_VERSION)
    extern unsigned int Image$$ER_IRAM_NS$$RW$$Base;
    extern unsigned int Load$$ER_IRAM_NS$$RW$$Base;
    extern unsigned int Image$$ER_IRAM_NS$$RW$$Length;

    section_image_base = (unsigned int *)& Image$$ER_IRAM_NS$$RW$$Base;
    section_load_base = (unsigned int *)& Load$$ER_IRAM_NS$$RW$$Base;
    section_image_length = (unsigned int)& Image$$ER_IRAM_NS$$RW$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__enter_iram_ns_rw_load_ad__;
    extern unsigned int *__enter_iram_ns_rw_length__;
    extern unsigned int *__enter_iram_ns_rw_start__;
    section_image_base = (unsigned int *)&__enter_iram_ns_rw_start__;
    section_load_base = (unsigned int *)& __enter_iram_ns_rw_load_ad__;
    section_image_length = (unsigned int)& __enter_iram_ns_rw_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memcpy(section_image_base, section_load_base, section_image_length);
#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("APP copy DTCM rw: src 0x%x, dest 0x%x, len %d",
               (uint32_t)section_load_base, (uint32_t)section_image_base, section_image_length);
#endif

    //clear dtcm zi
#if defined (__ARMCC_VERSION)
    extern unsigned int Image$$ER_IRAM_NS$$ZI$$Base;
    extern unsigned int Image$$ER_IRAM_NS$$ZI$$Length;

    section_image_base = (unsigned int *)& Image$$ER_IRAM_NS$$ZI$$Base;
    section_image_length = (unsigned int)& Image$$ER_IRAM_NS$$ZI$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__enter_iram_ns_zi_start__;
    extern unsigned int *__enter_iram_ns_zi_length__;

    section_image_base = (unsigned int *)& __enter_iram_ns_zi_start__;
    section_image_length = (unsigned int)& __enter_iram_ns_zi_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memset(section_image_base, 0, section_image_length);
#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("APP clear DTCM zi: addr 0x%x, len %d", (uint32_t)section_image_base,
               section_image_length);
#endif

    //copy data sram ro
#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
    extern unsigned int Image$$DATA_SRAM_SECTION$$RO$$Base;
    extern unsigned int Load$$DATA_SRAM_SECTION$$RO$$Base;
    extern unsigned int Image$$DATA_SRAM_SECTION$$RO$$Length;

    section_image_base = (unsigned int *)& Image$$DATA_SRAM_SECTION$$RO$$Base;
    section_load_base = (unsigned int *)& Load$$DATA_SRAM_SECTION$$RO$$Base;
    section_image_length = (unsigned int)& Image$$DATA_SRAM_SECTION$$RO$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__data_sram_section_ro_start__;
    extern unsigned int *__data_sram_section_ro_load_ad__;
    extern unsigned int *__data_sram_section_ro_length__;

    section_image_base = (unsigned int *)& __data_sram_section_ro_start__;
    section_load_base = (unsigned int *)& __data_sram_section_ro_load_ad__;
    section_image_length = (unsigned int)& __data_sram_section_ro_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memcpy(section_image_base, section_load_base, section_image_length);
#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("APP copy DATA SRAM RO: src 0x%x, dest 0x%x, len %d",
               (uint32_t)section_load_base, (uint32_t)section_image_base, section_image_length);
#endif

    //copy data sram rw
#if defined (__ARMCC_VERSION)
    extern unsigned int Image$$DATA_SRAM_SECTION$$RW$$Base;
    extern unsigned int Load$$DATA_SRAM_SECTION$$RW$$Base;
    extern unsigned int Image$$DATA_SRAM_SECTION$$RW$$Length;

    section_image_base = (unsigned int *)& Image$$DATA_SRAM_SECTION$$RW$$Base;
    section_load_base = (unsigned int *)& Load$$DATA_SRAM_SECTION$$RW$$Base;
    section_image_length = (unsigned int)& Image$$DATA_SRAM_SECTION$$RW$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__data_sram_section_rw_exe_ad__;
    extern unsigned int *__data_sram_section_rw_load_ad__;
    extern unsigned int *__data_sram_section_rw_length__;

    section_image_base = (unsigned int *)& __data_sram_section_rw_exe_ad__;
    section_load_base = (unsigned int *)& __data_sram_section_rw_load_ad__;
    section_image_length = (unsigned int)& __data_sram_section_rw_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memcpy(section_image_base, section_load_base, section_image_length);

#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("APP copy DATA SRAM rw: src 0x%x, dest 0x%x, len %d",
               (uint32_t)section_load_base, (uint32_t)section_image_base, section_image_length);
#endif

    //clear data sram zi
#if defined (__ARMCC_VERSION)
    extern unsigned int Image$$DATA_SRAM_SECTION$$ZI$$Base;
    extern unsigned int Image$$DATA_SRAM_SECTION$$ZI$$Length;

    section_image_base = (unsigned int *)& Image$$DATA_SRAM_SECTION$$ZI$$Base;
    section_image_length = (unsigned int)& Image$$DATA_SRAM_SECTION$$ZI$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__data_sram_section_zi_start__;
    extern unsigned int *__data_sram_section_zi_length__;

    section_image_base = (unsigned int *)& __data_sram_section_zi_start__;
    section_image_length = (unsigned int)& __data_sram_section_zi_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memset(section_image_base, 0, section_image_length);
#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("APP clear DATA SRAM zi: addr 0x%x, len %d", (uint32_t)section_image_base,
               section_image_length);
#endif

#if (USE_PSRAM == 1)
    //copy psram ro
#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
    extern unsigned int Image$$PSRAM_SECTION$$RO$$Base;
    extern unsigned int Load$$PSRAM_SECTION$$RO$$Base;
    extern unsigned int Image$$PSRAM_SECTION$$RO$$Length;
    section_image_base = (unsigned int *)& Image$$PSRAM_SECTION$$RO$$Base;
    section_load_base = (unsigned int *)& Load$$PSRAM_SECTION$$RO$$Base;
    section_image_length = (unsigned int)& Image$$PSRAM_SECTION$$RO$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__psram_section_ro_start__;
    extern unsigned int *__psram_section_ro_load_ad__;
    extern unsigned int *__psram_section_ro_length__;
    section_image_base = (unsigned int *)& __psram_section_ro_start__;
    section_load_base = (unsigned int *)& __psram_section_ro_load_ad__;
    section_image_length = (unsigned int)& __psram_section_ro_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memcpy(section_image_base, section_load_base, section_image_length);
    DBG_DIRECT("APP copy PSRAM RO: src 0x%x, dest 0x%x, len %d",
               (uint32_t)section_load_base, (uint32_t)section_image_base, section_image_length);

    //copy PSRAM rw
#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
    extern unsigned int Image$$PSRAM_SECTION$$RW$$Base;
    extern unsigned int Load$$PSRAM_SECTION$$RW$$Base;
    extern unsigned int Image$$PSRAM_SECTION$$RW$$Length;
    section_image_base = (unsigned int *)& Image$$PSRAM_SECTION$$RW$$Base;
    section_load_base = (unsigned int *)& Load$$PSRAM_SECTION$$RW$$Base;
    section_image_length = (unsigned int)& Image$$PSRAM_SECTION$$RW$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__psram_section_rw_exe_ad__;
    extern unsigned int *__psram_section_rw_load_ad__;
    extern unsigned int *__psram_section_rw_length__;
    section_image_base = (unsigned int *)& __psram_section_rw_exe_ad__;
    section_load_base = (unsigned int *)& __psram_section_rw_load_ad__;
    section_image_length = (unsigned int)& __psram_section_rw_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memcpy(section_image_base, section_load_base, section_image_length);
    DBG_DIRECT("APP copy PSRAM rw: src 0x%x, dest 0x%x, len %d",
               (uint32_t)section_load_base, (uint32_t)section_image_base, section_image_length);

    //clear PSRAM zi
#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
    extern unsigned int Image$$PSRAM_SECTION$$ZI$$Base;
    extern unsigned int Image$$PSRAM_SECTION$$ZI$$Length;
    section_image_base = (unsigned int *)& Image$$PSRAM_SECTION$$ZI$$Base;
    section_image_length = (unsigned int)& Image$$PSRAM_SECTION$$ZI$$Length;
#elif defined ( __GNUC__ )
    extern unsigned int *__psram_section_zi_start__;
    extern unsigned int *__psram_section_zi_length__;
    section_image_base = (unsigned int *)& __psram_section_zi_start__;
    section_image_length = (unsigned int)& __psram_section_zi_length__;
#endif
    // cppcheck-suppress nullPointer; objectIndex
    memset(section_image_base, 0, section_image_length);
    DBG_DIRECT("APP clear PSRAM zi: addr 0x%x, len %d", (uint32_t)section_image_base,
               section_image_length);
#endif
}

void wdg_system_reset_app_cb(WDTMode_TypeDef wdt_mode,
                             T_SW_RESET_REASON reset_reason) APP_FLASH_TEXT_SECTION ;
void wdg_system_reset_app_cb(WDTMode_TypeDef wdt_mode, T_SW_RESET_REASON reset_reason)
{
#if (FTL_POOL == 1)
    extern void ftl_cache_disable(void);
    ftl_cache_disable();
#endif

    //do something necessary before watch dog reset
    if (user_wdt_cb)
    {
        if (user_wdt_cb(wdt_mode, reset_reason))
        {
            return;
        }
    }

}

void set_os_clock(OS_TICK os_tick) APP_FLASH_TEXT_SECTION;
void set_os_clock(OS_TICK os_tick)
{
    switch (os_tick)
    {
    case OS_TICK_10MS:
        OTP->os_tick_rate_HZ = 100;
        break;
    case OS_TICK_5MS:
        OTP->os_tick_rate_HZ = 200;
        break;
    case OS_TICK_2MS:
        OTP->os_tick_rate_HZ = 500;
        break;
    case OS_TICK_1MS:
        OTP->os_tick_rate_HZ = 1000;
        break;
    default:
        break;
    }
}

static void AppUpdateVectorTable(void)
{
#if defined ( __ARMCC_VERSION )
    extern uint32_t Load$$RAM_VECTOR_TABLE$$RO$$Base;
    extern uint32_t Image$$RAM_VECTOR_TABLE$$RO$$Length;
#elif defined ( __GNUC__ )
    extern uint32_t *__ram_vector_load_ad__;
    extern uint32_t *__ram_vector_exe_ad__;
    extern uint32_t *__ram_vector_table_length__;
#endif

    extern void Default_Handler(void);
    const char *SysException[] =
    {
        "InitialSP", "Reset", "NMI", "HardFault", "MemManage", "BusFault", "UsageFault", "Rsvd",
        "Rsvd", "Rsvd", "Rsvd", "SVC", "DebugMon", "Rsvd", "PendSV", "SysTick"
    };
    const char *ExtIrq[] =
    {
        "System", "WDT", "RXI300", "RXI300_SEC", "Zigbee", "BTMAC", "Peripheral", "Rsvd", "RTC",
        "GPIO_A0", "GPIO_A1", "GPIO_A_2_7", "GPIO_A_8_15", "GPIO_A_16_23", "GPIO_A_24_31",
        "GPIO_B_0_7", "GPIO_B_8_15", "GPIO_B_16_23", "GPIO_B_24_31",
        "GDMA0 Channel0", "GDMA0 Channel1", "GDMA0 Channel2", "GDMA0 Channel3", "GDMA0 Channel4",
        "GDMA0 Channel5", "GDMA0 Channel6", "GDMA0 Channel7", "GDMA0 Channel8", "GDMA0 Channel9",
        "PPE", "I2C0", "I2C1", "I2C2", "I2C3", "UART0", "UART1", "UART2", "UART3", "UART4", "UART5",
        "SPI_3Wire", "SPI0", "SPI1", "SPI_Slave", "TIM0", "TIM1", "TIM2", "TIM3", "TIM4", "TIM5", "TIM6", "TIM7",
        "E_TIM0", "E_TIM1", "E_TIM2", "E_TIM3", "ADC_24bit", "SAR_ADC", "Keyscan", "AON_QDEC", "IR",
        "SDHC0", "ISO7816", "Display", "SPORT0 RX", "SPORT0 TX", "SPORT1 RX", "SPORT1 TX", "SHA256",
        "Pub Key Engine", "Flash SEC", "SegCom_CTL", "CAN", "ETH", "IDU", "Slave_Port_Monitor", "PTA_Mailbox",
        "USB", "USB_Suspend_N", "USB_Endp_Multi_Proc", "USB_IN_EP_0", "USB_IN_EP_1", "USB_IN_EP_2", "USB_IN_EP_3",
        "USB_IN_EP_4", "USB_IN_EP_5", "USB_OUT_EP_0", "USB_OUT_EP_1", "USB_OUT_EP_2", "USB_OUT_EP_3", "USB_OUT_EP_4",
        "USB_OUT_EP_5", "USB_Sof", "TMETER", "PF_RTC", "BTMAC_WRAP_AROUND", 
        "Timer_A0_A1", "BT_Bluewiz", "BTMAC rsvd0", "BT_BZ_DMA", "Proprietary_protocol", "BTMAC rsvd1",
        "SPIC0", "SPIC1", "SPIC2", "TRNG", "LPCOMP", "SPI PHY1",  "Peri rsvd",
        "GPIOA2",  "GPIOA3",  "GPIOA4",  "GPIOA5",  "GPIOA6",  "GPIOA7",
        "GPIOA8",  "GPIOA9",  "GPIOA10", "GPIOA11", "GPIOA12", "GPIOA13", "GPIOA14", "GPIOA15",
        "GPIOA16", "GPIOA17", "GPIOA18", "GPIOA19", "GPIOA20", "GPIOA21", "GPIOA22", "GPIOA23",
        "GPIOA24", "GPIOA25", "GPIOA26", "GPIOA27", "GPIOA28", "GPIOA29", "GPIOA30", "GPIOA31",
        "GPIOB0",  "GPIOB1",  "GPIOB2",  "GPIOB3",  "GPIOB4",  "GPIOB5",  "GPIOB6",  "GPIOB7",
        "GPIOB8",  "GPIOB9",  "GPIOB10", "GPIOB11", "GPIOB12", "GPIOB13", "GPIOB14", "GPIOB15",
        "GPIOB16", "GPIOB17", "GPIOB18", "GPIOB19", "GPIOB20", "GPIOB21", "GPIOB22", "GPIOB23",
        "GPIOB24", "GPIOB25", "GPIOB26", "GPIOB27", "GPIOB28", "GPIOB29", "GPIOB30", "GPIOB31",
    };
#if defined ( __ARMCC_VERSION )
#if (FEATURE_TRUSTZONE_ENABLE == 1)
    IRQ_Fun *pRamVector    = (IRQ_Fun *)NS_RAM_VECTOR_ADDR;
#else
    IRQ_Fun *pRamVector    = (IRQ_Fun *)S_RAM_VECTOR_ADDR;
#endif
    IRQ_Fun *pAppVector    = (IRQ_Fun *)&Load$$RAM_VECTOR_TABLE$$RO$$Base;
    uint32_t AppVectorSize = (uint32_t)&Image$$RAM_VECTOR_TABLE$$RO$$Length;
#elif defined (__GNUC__)
    IRQ_Fun *pRamVector    = (IRQ_Fun *)(&__ram_vector_exe_ad__);
    IRQ_Fun *pAppVector    = (IRQ_Fun *)(&__ram_vector_load_ad__);
    uint32_t AppVectorSize = (uint32_t)&__ram_vector_table_length__;
#endif
    uint32_t i = 0;

#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("SCB->VTOR=0x%x, RAM VTOR=0x%x, APP LOAD VTOR=0x%x, size=0x%x", SCB->VTOR, pRamVector,
               pAppVector, AppVectorSize);
    DBG_DIRECT("Rom default ISR 0x%x, APP Default ISR 0x%x", Default_Handler_rom, Default_Handler);
#endif

#if (FEATURE_TRUSTZONE_ENABLE == 1)
    //if(OTP->trustzone_enable)
    if (SCB->VTOR != NS_RAM_VECTOR_ADDR)   //VTOR_RAM_ADDR is ns rom vector addr
    {
        RamVectorTableInit(NS_RAM_VECTOR_ADDR);
    }
#else
    if (SCB->VTOR != S_RAM_VECTOR_ADDR)   //VTOR_RAM_ADDR is ns rom vector addr
    {
        RamVectorTableInit(S_RAM_VECTOR_ADDR);
    }
#endif
    /* Update APP defined handlers */
    for (i = 0; i < AppVectorSize / 4;  ++i)
    {
        if (i == 0 || i == 1) //skip __initial_sp and reset_handler remap
        {
            continue;
        }

        //DBG_DIRECT("before update: IPSR %d, app 0x%x, raw vector 0x%x", i, pAppVector[i], pRamVector[i]);

        if (((pAppVector[i] != Default_Handler) && (pAppVector[i] != 0)) ||
            ((pAppVector[i] == Default_Handler) &&
             (pRamVector[i] == Default_Handler_rom)))  //rom default handler
        {
#if (BOOT_DIRECT_DEBUG == 1)
            if (i < System_VECTORn)
            {
                DBG_DIRECT("Warning! %s is updated by APP!", SysException[i]);
            }
            else
            {
                DBG_DIRECT("Warning! ISR %s is updated by APP!", ExtIrq[i - System_VECTORn]);
            }
#else
            if (i < System_VECTORn)
            {
                BOOT_PRINT_WARN1("Warning! %s is updated by APP!", TRACE_STRING(SysException[i]));
            }
            else
            {
                BOOT_PRINT_WARN1("Warning! ISR %s is updated by APP!",
                                 TRACE_STRING(ExtIrq[i - System_VECTORn]));
            }
#endif
            RamVectorTableUpdate_ext((VECTORn_Type)i, (IRQ_Fun)pAppVector[i]);
        }

    }
    __DMB();
    __DSB();
}

uint32_t reset_reason_print(void) APP_FLASH_TEXT_SECTION;
uint32_t reset_reason_print(void)
{
    uint32_t result = 0;

    AON_NS_REG0X_APP_TYPE aon_0x1ae0 = {.d32 = AON_REG_READ(AON_NS_REG0X_APP)};
    sw_reset_reason = aon_0x1ae0.reset_reason;
    aon_0x1ae0.reset_reason = RESET_REASON_HW;
    AON_REG_WRITE(AON_NS_REG0X_APP, aon_0x1ae0.d32);

    AON_NS_REG_KM4_WDT_MODE_TYPE wdg_reset_flag;
    wdg_reset_flag.d32 = AON_REG_READ(AON_NS_REG_KM4_WDT_MODE);

    uint32_t wdg_reset_pc = AON_REG_READ(AON_NS_REG30_CORE_KM4_PC);
    uint32_t wdg_reset_lr = AON_REG_READ(AON_NS_REG34_CORE_KM4_LR);
    uint32_t wdg_reset_psr = AON_REG_READ(AON_NS_REG3C_CORE_KM4_XPSR_R);

    AON_NS_REG_AON_WDT_MODE_TYPE aon_wdg_reset_flag;
    aon_wdg_reset_flag.d32 = AON_REG_READ(AON_NS_REG_AON_WDT_MODE);

    uint32_t aon_wdg_reset_pc = AON_REG_READ(AON_NS_REG50_AON_KM4_PC);
    uint32_t aon_wdg_reset_lr = AON_REG_READ(AON_NS_REG54_AON_KM4_LR);
    uint32_t aon_wdg_reset_psr = AON_REG_READ(AON_NS_REG5C_AON_KM4_XPSR_R);

    BOOT_PRINT_INFO1("[Debug Info]RESET Reason: SW, TYPE 0x%x", sw_reset_reason);
    BOOT_PRINT_INFO2("[Debug Info]WDG timeout flag 0x%x, WDG mode 0x%x",
                     wdg_reset_flag.r_km4_wdt_timeout_flag, wdg_reset_flag.r_km4_wdt_mode);
    BOOT_PRINT_INFO2("[Debug Info]AON WDG timeout flag 0x%x,AON WDG mode 0x%x",
                     aon_wdg_reset_flag.r_aon_wdt_timeout_flag, aon_wdg_reset_flag.r_aon_wdt_mode);

    BOOT_PRINT_INFO3("[Debug Info]WDG: pc %x, lr %x, psr %x", wdg_reset_pc, wdg_reset_lr,
                     wdg_reset_psr);
    BOOT_PRINT_INFO3("[Debug Info]AON WDG: pc %x, lr %x, psr %x", aon_wdg_reset_pc, aon_wdg_reset_lr,
                     aon_wdg_reset_psr);

    if (wdg_reset_flag.r_km4_wdt_timeout_flag == 0 &&
        aon_wdg_reset_flag.r_aon_wdt_timeout_flag == 0)
    {
        if (sw_reset_reason == RESET_REASON_HW)
        {
            BOOT_PRINT_INFO0("[Debug Info]HW Reset");
            wdt_reset_type = HW_RESET;
            result = ((uint16_t)wdt_reset_type << 8) | sw_reset_reason;
        }
        else if (sw_reset_reason == RESET_REASON_DLPS)
        {
            BOOT_PRINT_INFO0("[Debug Info]AON WDT Timeout Reset in DLPS");
            wdt_reset_type = AON_WDT_RESET_DLPS;
            result = ((uint16_t)wdt_reset_type << 8) | sw_reset_reason;
        }
    }
    else
    {
        if (sw_reset_reason == RESET_REASON_HW)
        {
            if (wdg_reset_flag.r_km4_wdt_timeout_flag == 1 && wdg_reset_flag.r_km4_wdt_mode == 0)
            {
                BOOT_PRINT_INFO0("[Debug Info]Core WDT Timeout Reset, level 0");
                wdt_reset_type = CORE_WDT_RESET_LV0;
                sw_reset_reason = RESET_REASON_WDT_TIMEOUT;
                result = ((uint16_t)wdt_reset_type << 8) | sw_reset_reason;
            }
            else if (wdg_reset_flag.r_km4_wdt_timeout_flag == 1 && wdg_reset_flag.r_km4_wdt_mode == 1)
            {
                BOOT_PRINT_INFO0("[Debug Info]Core WDT Timeout Reset, level 1");
                wdt_reset_type = CORE_WDT_RESET_LV1;
                sw_reset_reason = RESET_REASON_WDT_TIMEOUT;
                result = ((uint16_t)wdt_reset_type << 8) | sw_reset_reason;
            }
            else if (aon_wdg_reset_flag.r_aon_wdt_timeout_flag == 1 && aon_wdg_reset_flag.r_aon_wdt_mode == 0)
            {
                BOOT_PRINT_INFO0("[Debug Info]AON WDT Timeout Reset, level 0");
                wdt_reset_type = AON_WDT_RESET_LV0;
                sw_reset_reason = RESET_REASON_WDT_TIMEOUT;
                result = ((uint16_t)wdt_reset_type << 8) | sw_reset_reason;
            }
            else if (aon_wdg_reset_flag.r_aon_wdt_timeout_flag == 1 && aon_wdg_reset_flag.r_aon_wdt_mode == 1)
            {
                BOOT_PRINT_INFO0("[Debug Info]AON WDT Timeout Reset, level 1");
                wdt_reset_type = AON_WDT_RESET_LV1;
                sw_reset_reason = RESET_REASON_WDT_TIMEOUT;
                result = ((uint16_t)wdt_reset_type << 8) | sw_reset_reason;
            }
        }
        else
        {
            BOOT_PRINT_INFO1("[Debug Info]System Reset, TYPE 0x%x", sw_reset_reason);
            wdt_reset_type = SYSTEM_RESET_REASON;
            result = ((uint16_t)wdt_reset_type << 8) | sw_reset_reason;
        }
    }

    return result;
}

void pre_main(void) APP_FLASH_TEXT_SECTION;
void pre_main(void)
{
    uint32_t cpu_clk = 0;
    pm_cpu_freq_get(&cpu_clk);
#if (BOOT_DIRECT_DEBUG == 1)
    DBG_DIRECT("APP pre_main, Timestamp: %d us", read_cpu_counter() / cpu_clk);
#endif
    __disable_irq();

    reset_reason_print();

    setlocale(LC_ALL, "C");

    BOOT_PRINT_ERROR1("SDK Ver: %s",
                      TRACE_STRING(VERSION_BUILD_STR));

    AppUpdateVectorTable();

#if (DEBUG_WATCHPOINT_ENABLE == 1)
    debug_monitor_enable();
#endif

#if (DEFAULT_FLASH_BP_LEVEL_ENABLE == 1)
    fmc_flash_set_default_bp_lv(true);
#else
    fmc_flash_set_default_bp_lv(false);
#endif

    if (app_pre_main_cb)
    {
        app_pre_main_cb();
    }

    return;
}

bool system_init(void) APP_FLASH_TEXT_SECTION;
bool system_init(void)
{

    //hci mode check and bypass app
    if (OTP->stack_en == 0)
    {
        DBG_DIRECT("WARNING: In HCI Mode, will not run APP Task, Timestamp: %d us",
                   read_cpu_counter() / 40);
        return true;
    }
    else
    {
        DBG_DIRECT("In SoC Mode, Timestamp: %d us", read_cpu_counter() / 40);
    }

#if (BOOT_DIRECT_DEBUG == 1)
    if (FEATURE_TRUSTZONE_ENABLE)
    {
        DBG_DIRECT("Non-secure APP Entry");
    }
    else
    {
        DBG_DIRECT("Secure APP Entry");
    }

#endif

#if (USE_PSRAM == 1)
    psram_winbond_opi_init();
#endif

    ram_init();

    bool use_psram = fmc_get_psram_power_status();
#if (USE_PSRAM == 1)
    if (use_psram == 0)
    {
        DBG_DIRECT("psram is initialized but not powered on!");
    }
#else
    if (use_psram == 1)
    {
        DBG_DIRECT("psram is powered on but not initialized!");
    }
#endif

    //init pre_main and main functions
    app_pre_main = (APP_MAIN_FUNC)pre_main;
    app_main = (APP_MAIN_FUNC)common_main;

    wdg_register_app_cb((WDG_APP_CB)wdg_system_reset_app_cb);

    //add update OTP here

    //sw timer config
#ifdef TIMER_MAX_NUMBER
    //define TIMER_MAX_NUMBER in otp_config.h
    OTP->timerMaxNumber = TIMER_MAX_NUMBER;
#endif

    //timer task stack size config
#ifdef TIMER_TASK_STACK_SIZE
    //define TIMER_TASK_STACK_SIZE in otp_config.h
    OTP->timer_task_stack_size = TIMER_TASK_STACK_SIZE;
#endif

    //cpu_sleep_en config
#ifdef CPU_SLEEP_EN
    //define CPU_SLEEP_EN in otp_config.h
    OTP->cpu_sleep_en = CPU_SLEEP_EN;
#endif

    //os tick time config
#ifdef OS_TICK_TIME
    //define OS_TICK_TIME in otp_config.h
    set_os_clock(OS_TICK_TIME);
#endif

    //ftl config
#ifdef USE_FTL
    //define USE_FTL in otp_config.h
    OTP->use_ftl = USE_FTL;
#endif

    // swd pinmux config
#ifdef SWD_PINMUX_ENABLE
    //define SWD_PINMUX_ENABLE in otp_config.h
#if (SWD_PINMUX_ENABLE == 0)
    swd_pin_disable();
#endif
#endif

    //add multi-OS support
#if (MULT_OS == USE_FREERTOS)
    extern void os_freertos_patch_init(void);
    os_patch_init = os_freertos_patch_init;
#elif (MULT_OS == USE_RTT)   // RT-Thread OS
    extern void os_rtt_patch_init(void);
    os_patch_init = os_rtt_patch_init;
#endif

    //heap_ext_data_sram_mask_en config
#ifdef HEAP_EXT_DATA_SRAM_MASK_EN
    //define HEAP_EXT_DATA_SRAM_MASK_EN in otp_config.h
    OTP->heap_mask.heap_ext_data_sram_mask = HEAP_EXT_DATA_SRAM_MASK_EN;
#endif

    //log_ram_size config
#ifdef LOG_RAM_SIZE
    //define LOG_RAM_SIZE in otp_config.h
    OTP->log_ram_size = LOG_RAM_SIZE;
#endif

    return true;
}

void Peripheral_Handler(void) APP_RAM_TEXT_SECTION;
void Peripheral_Handler(void)
{
    //DBG_DIRECT("Peri_Handler");
    IRQ_Fun *Vectors = (IRQ_Fun *)SCB->VTOR;

    uint32_t peripheral_interrupt_status = SoC_VENDOR->u_004.REG_LOW_PRI_INT_STATUS;
    uint32_t real_interrupt_status = peripheral_interrupt_status &
                                     (SoC_VENDOR->u_00C.REG_LOW_PRI_INT_EN);

    while (real_interrupt_status)
    {
        uint8_t idx = 31 - __builtin_clz(real_interrupt_status);
        Vectors[Peripheral_First_VECTORn + idx]();
        real_interrupt_status &= ~BIT(idx);
    }

    //Clear sub-rout IRQ
    HAL_WRITE32(SOC_VENDOR_REG_BASE, 0x4, peripheral_interrupt_status);
}

void GPIOA_Handler(uint32_t mask) APP_RAM_TEXT_SECTION;
void GPIOA_Handler(uint32_t mask)
{
    IRQ_Fun *Vectors = (IRQ_Fun *)SCB->VTOR;
    uint32_t real_interrupt_status = GPIOA->GPIO_INT_STS & mask ;
    while (real_interrupt_status)
    {
        uint8_t idx = 31 - __builtin_clz(real_interrupt_status);
        Vectors[GPIOA2_VECTORn + idx - 2]();
        real_interrupt_status &= ~BIT(idx);
    }
}

void GPIO_A_2_7_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_A_2_7_Handler(void)
{
    GPIOA_Handler(0xFC);
}

void GPIO_A_8_15_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_A_8_15_Handler(void)
{
    GPIOA_Handler(0xFF00);
}

void GPIO_A_16_23_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_A_16_23_Handler(void)
{
    GPIOA_Handler(0xFF0000);
}

void GPIO_A_24_31_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_A_24_31_Handler(void)
{
    GPIOA_Handler(0xFF000000);
}

void GPIOB_Handler(uint32_t mask) APP_RAM_TEXT_SECTION;
void GPIOB_Handler(uint32_t mask)
{
    IRQ_Fun *Vectors = (IRQ_Fun *)SCB->VTOR;
    uint32_t real_interrupt_status = GPIOB->GPIO_INT_STS & mask ;
    while (real_interrupt_status)
    {
        uint8_t idx = 31 - __builtin_clz(real_interrupt_status);
        Vectors[GPIOB0_VECTORn + idx]();
        real_interrupt_status &= ~BIT(idx);
    }
}

void GPIO_B_0_7_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_B_0_7_Handler(void)
{
    GPIOB_Handler(0xFF);
}

void GPIO_B_8_15_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_B_8_15_Handler(void)
{
    GPIOB_Handler(0xFF00);
}

void GPIO_B_16_23_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_B_16_23_Handler(void)
{
    GPIOB_Handler(0xFF0000);
}

void GPIO_B_24_31_Handler(void) APP_RAM_TEXT_SECTION;
void GPIO_B_24_31_Handler(void)
{
    GPIOB_Handler(0xFF000000);
}

#if defined (__ARMCC_VERSION)
/*retarget to weak function to ensure that the application has freedom of redefinition*/
__WEAK void *malloc(size_t size)
{
    return os_mem_alloc(RAM_TYPE_DATA_ON, size);
}

__WEAK void *calloc(size_t n, size_t size)
{
    return os_mem_zalloc(RAM_TYPE_DATA_ON, n * size);
}

__WEAK void *realloc(void *ptr, size_t size)
{
    if (ptr)
    {
        os_mem_free(ptr);
    }

    return os_mem_alloc(RAM_TYPE_DATA_ON, size);
}

__WEAK void free(void *ptr)
{
    os_mem_free(ptr);
}
#endif

#if defined ( __ARMCC_VERSION   )
#elif defined ( __GNUC__   )
extern void *__aeabi_memset(void *s, size_t n, int c);
void *memset(void *s, int c, size_t n)
{
    return  __aeabi_memset(s, n, c);
}
#endif

#if defined ( __ARMCC_VERSION   )
#elif defined ( __GNUC__   )
#include <sys/stat.h>
#include <errno.h>
#undef errno
extern int errno;

int __attribute__((weak)) _close(int file)
{
    return -1;
}

int __attribute__((weak)) _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int __attribute__((weak)) _getpid(void)
{
    return 1;
}

int __attribute__((weak)) _isatty(int file)
{
    return 1;
}

int __attribute__((weak)) _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

int __attribute__((weak)) _lseek(int file, int ptr, int dir)
{
    return 0;
}

int __attribute__((weak)) _read(int file, char *ptr, int len)
{
    return 0;
}

int __attribute__((weak)) _write(int file, char *ptr, int len)
{
    return len;
}
#endif
