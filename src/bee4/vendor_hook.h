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

#ifndef __VENDOR_HOOK_H
#define __VENDOR_HOOK_H

/*============================================================================*
 *                        Header Files
 *============================================================================*/
#include "ncp_base.hpp"
#include "zb_tst_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif
/*============================================================================*
 *                              Macros
 *============================================================================*/

/*============================================================================*
 *                         Types
 *============================================================================*/
enum
{
    SPINEL_PROP_VENDOR_RTK__BEGIN = (SPINEL_PROP_VENDOR__BEGIN + 0),
    SPINEL_PROP_VENDOR_RTK_SIGN_RANDOM = (SPINEL_PROP_VENDOR__BEGIN + 1),
    SPINEL_PROP_VENDOR_RTK_SIGN_DATA = (SPINEL_PROP_VENDOR__BEGIN + 2),
    SPINEL_PROP_VENDOR_RTK_ENTER_CFU_MODE = (SPINEL_PROP_VENDOR_RTK__BEGIN + 3),
    SPINEL_PROP_VENDOR_RTK_PTA_MODE = (SPINEL_PROP_VENDOR_RTK__BEGIN + 4),
    SPINEL_PROP_VENDOR_RTK_READ_REGISTER = (SPINEL_PROP_VENDOR_RTK__BEGIN + 5),
    SPINEL_PROP_VENDOR_RTK_WRITE_REGISTER = (SPINEL_PROP_VENDOR_RTK__BEGIN + 6),
    SPINEL_PROP_VENDOR_RTK_VID_PID = (SPINEL_PROP_VENDOR_RTK__BEGIN + 7),
    SPINEL_PROP_VENDOR_RTK_VERSION = (SPINEL_PROP_VENDOR_RTK__BEGIN + 8),
    SPINEL_PROP_VENDOR_RTK_DATA1_VERSION = (SPINEL_PROP_VENDOR_RTK__BEGIN + 9),
    SPINEL_PROP_VENDOR_RTK_CONFIG_READ = (SPINEL_PROP_VENDOR_RTK__BEGIN + 10),
    SPINEL_PROP_VENDOR_RTK_CONFIG_WTITE = (SPINEL_PROP_VENDOR_RTK__BEGIN + 11),
    SPINEL_PROP_VENDOR_RTK_FLOW_CONTROL = (SPINEL_PROP_VENDOR_RTK__BEGIN + 12),
    //add more below


    SPINEL_PROP_VENDOR_RTK__END   = (SPINEL_PROP_VENDOR__BEGIN + 0x400),
};

typedef struct
{
    union
    {
        uint32_t version;
        struct
        {
            uint32_t _version_reserve: 8;   //!< reserved
            uint32_t _version_revision: 8; //!< revision version
            uint32_t _version_minor: 8;     //!< minor version
            uint32_t _version_major: 8;     //!< major version
        } header_sub_version; //!< ota header sub version
        struct
        {
            uint32_t _version_major: 4;     //!< major version
            uint32_t _version_minor: 8;     //!< minor version
            uint32_t _version_revision: 15; //!< revision version
            uint32_t _version_reserve: 5;   //!< reserved
        } img_sub_version; //!< other image sub version including patch, app, app data1-6

    } ver_info;
} T_IMAGE_VERSION;

typedef enum
{
    IMG_SCCD             = 0x379D,
    IMG_OCCD             = 0x379E,
    IMG_BOOTPATCH        = 0x379F,          /*!< KM4 boot patch */
    IMG_DFU_FIRST        = IMG_BOOTPATCH,
    IMG_OTA              = 0x37A0,          /*!< OTA header */
    IMG_SECUREMCUAPP     = 0x37A2,          /*!< KM4 secure app */
    IMG_SECUREMCUAPPDATA = 0x37A3,          /*!< KM4 secure app data */
    IMG_BT_STACKPATCH    = 0x37A6,          /*!< BT stack patch */
    IMG_BANK_FIRST       = IMG_BT_STACKPATCH,
    IMG_MCUPATCH         = 0x37A7,          /*!< KM4 non-secure rom patch */
    IMG_UPPERSTACK       = 0x37A8,          /*!< KM4 non-secure Upperstack */
    IMG_MCUAPP           = 0x37A9,          /*!< KM4 non-secure app */
    IMG_MCUCFGDATA       = 0x37AA,          /*!< KM4 MCUConfigData */
    IMG_MCUAPPDATA1      = 0x37AE,
    IMG_MCUAPPDATA2      = 0x37AF,
    IMG_MCUAPPDATA3      = 0x37B0,
    IMG_MCUAPPDATA4      = 0x37B1,
    IMG_MCUAPPDATA5      = 0x37B2,
    IMG_MCUAPPDATA6      = 0x37B3,
    IMG_ZIGBEESTACK      = 0x37B4,          /*!< KM4 Zigbee stack */
    IMG_MAX              = 0x37B5,
    IMG_DFU_MAX          = IMG_MAX,

    IMG_RO_DATA1         = 0x3A81,  /*!< FOR vp */
    IMG_RO_DATA2         = 0x3A82,
    IMG_RO_DATA3         = 0x3A83,
    IMG_RO_DATA4         = 0x3A84,
    IMG_RO_DATA5         = 0x3A85,
    IMG_RO_DATA6         = 0x3A86,

    IMG_USER_DATA8       = 0xFFF7,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA_FIRST  = IMG_USER_DATA8,
    IMG_USER_DATA7       = 0xFFF8,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA6       = 0xFFF9,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA5       = 0xFFFA,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA4       = 0xFFFB,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA3       = 0xFFFC,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA2       = 0xFFFD,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA1       = 0xFFFE,    /*!< the image data only support unsafe single bank ota */
    IMG_USER_DATA_MAX    = 0xFFFF,    /*!< the image data only support unsafe single bank ota */
} IMG_ID;

/*============================================================================*
*                        Export Global Variables
*============================================================================*/
extern rtk_config_param config_param;

/*============================================================================*
 *                         Functions
 *============================================================================*/
#if FEATURE_SUPPORT_RTK_SIGN
extern void rtk_sign_get_sign_data(uint8_t *result);
extern void rtk_sign_set_random_data(const uint8_t *data, uint8_t data_len);
#endif
#if FEATURE_SUPPORT_CFU
extern void enter_to_cfu_mode(const uint8_t *data, uint8_t data_len);
#endif
extern void mac_PTA_Enable(uint8_t enable);
extern void mac_read_reg(uint32_t addr, uint8_t *value);
extern void mac_write_reg(const char *addr, const char *value);
extern void get_ota_bank_image_version(bool a, IMG_ID b, T_IMAGE_VERSION *c);
extern uint32_t get_active_bank_image_size_by_img_id(IMG_ID d);
extern bool rtk_write_config_param(const uint8_t *data, uint8_t data_len);
extern bool rtk_enable_flow_control(const uint8_t *data, uint8_t data_len);

#ifdef __cplusplus
}
#endif

#endif /*__VENDOR_HOOK_H*/

/******************* (C) COPYRIGHT 2024 Realtek Semiconductor Corporation *****END OF FILE****/

