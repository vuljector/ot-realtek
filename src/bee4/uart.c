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
 *   This file implements the OpenThread platform abstraction for UART communication.
 *
 */

#include "platform-bee.h"
#include <openthread-core-config.h>
#include <openthread/config.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include <openthread/platform/debug_uart.h>
#include <openthread/platform/logging.h>
#include "openthread-system.h"

#include "utils/code_utils.h"
#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
#include "utils/uart.h"
#else
#include <openthread/platform/uart.h>
#endif
#include "mac_driver.h"
#if(1 == BUILD_RCP || 1 == MATTER_ENABLE_CFU)
#include "board.h"
#endif
#if (FEATURE_SUPPORT_CFU && (USE_USB_CDC == USB_TYPE))
#include "cfu.h"
#include "usb_cfu_handle.h"
#include "cfu_application.h"
#endif
#ifdef BOARD_RTL8771GTV
#include "zb_tst_cfg.h"
#endif

enum
{
    kPlatformClock     = 32000000,
    kBaudRate          = 115200,
    kReceiveBufferSize = 512,
};

#if (ENABLE_CLI == 1 || BUILD_NCP == 1 || BUILD_RCP == 1 || ENABLE_PW_RPC == 1)

static volatile uint16_t curr_rx_tail = 0;
static volatile uint16_t rx_tail = 0;
static volatile uint16_t rx_head = 0;
static uint8_t rx_buffer[kReceiveBufferSize];

static const uint8_t    *sTransmitBuffer = NULL;
static volatile bool    sTransmitDone = false;
static void *uart_tx_mutex = NULL;
static void *uart_tx_sem = NULL;

#ifdef BUILD_USB

#include "dbg_printf.h"
#include "utils.h"
#include "cdc.h"
#include "usb_spec20.h"
#include "usb_dm.h"
#include "usb_dev_driver.h"
#include "usb_cdc_driver.h"

#define  CDC_BULK_IN_EP                  0x82
#define  CDC_BULK_OUT_EP                 0x02

static void *cdc_in_handle = NULL;
static void *cdc_out_handle = NULL;
#if (FEATURE_SUPPORT_CFU && (USE_USB_CDC == USB_TYPE))
T_CDC_PACKET_DEF cdc_packet;
#endif
static uint8_t usb_speed_mode = USB_SPEED_FULL;


typedef enum
{
    STRING_ID_UNDEFINED,
    STRING_ID_MANUFACTURER,
    STRING_ID_PRODUCT,
    STRING_ID_SERIALNUM,
} T_STRING_ID;

static T_USB_DEVICE_DESC usb_dev_desc =
{
    .bLength                = sizeof(T_USB_DEVICE_DESC),
    .bDescriptorType        = USB_DESC_TYPE_DEVICE,
    .bcdUSB                 = 0x0200,
    .bDeviceClass           = 0x02,
    .bDeviceSubClass        = 0x00,
    .bDeviceProtocol        = 0x00,
    .bMaxPacketSize0        = 0x40,
    .idVendor               = 0x0bda,
    .idProduct              = 0x8777,
    .bcdDevice              = 0x0200,
    .iManufacturer          = STRING_ID_MANUFACTURER,
    .iProduct               = STRING_ID_PRODUCT,
    .iSerialNumber          = STRING_ID_SERIALNUM,
    .bNumConfigurations     = 1,
};

static T_STRING dev_strings[] =
{
    [0] =
    {
        .id     = STRING_ID_MANUFACTURER,
        .s = "RealTek",
    },
    [1] =
    {
        .id     = STRING_ID_PRODUCT,
        .s = "USB Dongle",
    },
    [2] =
    {
        .id     = STRING_ID_SERIALNUM,
        .s = "0123456789A",
    },
    [3] =
    {
        .id     = STRING_ID_UNDEFINED,
        .s = NULL,
    },
};

T_STRING_TAB dev_stringtab =
{
    .language = 0x0409,
    .strings = dev_strings,
};

const T_STRING_TAB *const dev_stringtabs[] =
{
    [0] = &dev_stringtab,
    [1] = NULL,
};

static T_USB_CONFIG_DESC usb_cfg_desc =
{
    .bLength = sizeof(T_USB_CONFIG_DESC),
    .bDescriptorType = USB_DESC_TYPE_CONFIG,
    .wTotalLength = 0xFFFF, //wTotalLength will be recomputed in usb lib according total interface descriptors
    .bNumInterfaces = 2, //bNumInterfaces will be recomputed in usb lib according total interface num
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = USB_ATTRIBUTE_ALWAYS_ONE | USB_ATTRIBUTE_SELFPOWERED,
    .bMaxPower = 250
};

static T_USB_INTERFACE_DESC cdc_std_if_desc =
{
    .bLength            = sizeof(T_USB_INTERFACE_DESC),
    .bDescriptorType    = USB_DESC_TYPE_INTERFACE,
    .bInterfaceNumber   = 0,
    .bAlternateSetting  = 0,
    .bNumEndpoints      = 1,
    .bInterfaceClass    = USB_CLASS_CODE_COMM,
    .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
    .bInterfaceProtocol = 0,
    .iInterface         = 0,
};

static T_CDC_HEADER_FUNC_DESC cdc_header_desc =
{
    .bFunctionLength = sizeof(T_CDC_HEADER_FUNC_DESC),
    .bDescriptorType = USB_DESCTYPE_CLASS_INTERFACE,
    .bDescriptorSubtype = 0,
    .bcdCDC = 0x0110
};

static T_CM_FUNC_DESC cm_func_desc =
{
    .bFunctionLength = sizeof(T_CM_FUNC_DESC),
    .bDescriptorType = USB_DESCTYPE_CLASS_INTERFACE,
    .bDescriptorSubtype = USB_CDC_SUBCLASS_CM,
    .bmCapabilities = 0x03,
    .bDataInterface = 1
};

static T_CDC_ACM_FUNC_DESC acm_func_desc =
{
    .bFunctionLength = sizeof(T_CDC_ACM_FUNC_DESC),
    .bDescriptorType = USB_DESCTYPE_CLASS_INTERFACE,
    .bDescriptorSubtype = USB_CDC_SUBCLASS_ACM,
    .bmCapabilities = 0x02,
};

static T_CDC_UNION_FUNC_DESC cdc_union_desc =
{
    .bFunctionLength = sizeof(T_CDC_UNION_FUNC_DESC),
    .bDescriptorType = USB_DESCTYPE_CLASS_INTERFACE,
    .bDescriptorSubtype = 0x06,
    .bControlInterface = 0,   // intf num of cdc_std_if_desc
    .bSubordinateInterface = 1   // intf num of cdc_std_data_if_desc
};

static T_USB_ENDPOINT_DESC int_in_ep_desc_fs =
{
    .bLength           = sizeof(T_USB_ENDPOINT_DESC),
    .bDescriptorType   = USB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress  = USB_DIR_IN | 0x01,
    .bmAttributes      = USB_EP_TYPE_INT,
    .wMaxPacketSize    = 0x40,
    .bInterval         = 1,
};

static T_USB_ENDPOINT_DESC int_in_ep_desc_hs =
{
    .bLength           = sizeof(T_USB_ENDPOINT_DESC),
    .bDescriptorType   = USB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress  = USB_DIR_IN | 0x01,
    .bmAttributes      = USB_EP_TYPE_INT,
    .wMaxPacketSize    = 0x40,
    .bInterval         = 4,
};

static T_USB_INTERFACE_DESC cdc_std_data_if_desc =
{
    .bLength = sizeof(T_USB_INTERFACE_DESC),
    .bDescriptorType = USB_DESC_TYPE_INTERFACE,
    .bInterfaceNumber = 1,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_CDC_DATA,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0, // Common AT commands
    .iInterface = 0
};

static T_USB_ENDPOINT_DESC bulk_in_ep_desc_fs =
{
    .bLength           = sizeof(T_USB_ENDPOINT_DESC),
    .bDescriptorType   = USB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress  = USB_DIR_IN | 0x02,
    .bmAttributes      = USB_EP_TYPE_BULK,
    .wMaxPacketSize    = 0x40,
    .bInterval         = 0,
};

static T_USB_ENDPOINT_DESC bulk_in_ep_desc_hs =
{
    .bLength           = sizeof(T_USB_ENDPOINT_DESC),
    .bDescriptorType   = USB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress  = USB_DIR_IN | 0x02,
    .bmAttributes      = USB_EP_TYPE_BULK,
    .wMaxPacketSize    = 512,
    .bInterval         = 0,
};

static T_USB_ENDPOINT_DESC bulk_out_ep_desc_fs =
{
    .bLength           = sizeof(T_USB_ENDPOINT_DESC),
    .bDescriptorType   = USB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress  = USB_DIR_OUT | 0x02,
    .bmAttributes      = USB_EP_TYPE_BULK,
    .wMaxPacketSize    = 0x40,
    .bInterval         = 0,
};

static T_USB_ENDPOINT_DESC bulk_out_ep_desc_hs =
{
    .bLength           = sizeof(T_USB_ENDPOINT_DESC),
    .bDescriptorType   = USB_DESC_TYPE_ENDPOINT,
    .bEndpointAddress  = USB_DIR_OUT | 0x02,
    .bmAttributes      = USB_EP_TYPE_BULK,
    .wMaxPacketSize    = 512,
    .bInterval         = 0,
};

static void *cdc_if0_descs_fs[] =
{
    (void *) &cdc_std_if_desc,
    (void *) &cdc_header_desc,
    (void *) &cm_func_desc,
    (void *) &acm_func_desc,
    (void *) &cdc_union_desc,
    (void *) &int_in_ep_desc_fs,
    NULL,
};

static void *cdc_if0_descs_hs[] =
{
    (void *) &cdc_std_if_desc,
    (void *) &cdc_header_desc,
    (void *) &cm_func_desc,
    (void *) &acm_func_desc,
    (void *) &cdc_union_desc,
    (void *) &int_in_ep_desc_hs,
    NULL,
};

static void *cdc_if1_descs_fs[] =
{
    (void *) &cdc_std_data_if_desc,
    (void *) &bulk_in_ep_desc_fs,
    (void *) &bulk_out_ep_desc_fs,
    NULL,
};

static void *cdc_if1_descs_hs[] =
{
    (void *) &cdc_std_data_if_desc,
    (void *) &bulk_in_ep_desc_hs,
    (void *) &bulk_out_ep_desc_hs,
    NULL,
};

static volatile bool usb_uart_ready = false;
void SetControlLineStateCb(uint16_t wValue)
{
    DBG_DIRECT("SetControlLineStateCb %x", wValue);
    usb_uart_ready = (wValue & 0x0001) ? true : false;
}

void SetLineCondigCb(T_LINE_CODING Line_condig)
{
    DBG_DIRECT("SetLineCondigCb");
}

void GetLineCondigCb(void)
{
    DBG_DIRECT("GetLineCondigCb");
}

static volatile uint16_t sRemainLength = 0;
static volatile uint16_t sTransmitLength = 0;

void CdcSendDataCb(void *handle, void *buf, uint32_t len, int status)
{
    os_sem_give(uart_tx_sem);
}

void CdcRecvDataCb(void *handle, void *buf, uint32_t len, int status)
{
#if (FEATURE_SUPPORT_CFU && (USE_USB_CDC == USB_TYPE))
    if (is_app_in_cfu_mode == true)
    {
        if (len > CFU_CMD_SIZE)
        {
            len = CFU_CMD_SIZE;
        }
        memcpy(cdc_packet.cdc_buf, buf, len);
        cdc_packet.payload_len = len;
        /* Send Msg to App task */
        T_IO_MSG uart_handle_msg;
        uart_handle_msg.type  = IO_MSG_TYPE_UART;
        uart_handle_msg.subtype = IO_MSG_UART_RX;
        uart_handle_msg.u.buf    = (void *)(&cdc_packet);
        extern bool app_send_msg_to_apptask(T_IO_MSG * p_msg);
        if (false == app_send_msg_to_apptask(&uart_handle_msg))
        {
            APP_PRINT_INFO0("[data_uart_handle_interrupt_handler] Send IO_MSG_TYPE_UART message failed!");
        }
    }
    else
#endif
    {
        uint8_t *recv = (uint8_t *)buf;

#if MATTER_ENABLE_CFU
        extern void matter_ble_send_msg(uint16_t sub_type, uint32_t param);

#define ENTER_CFU_MODE_DATA_LEN 13
#define MATTER_BLE_MSG_ENTER_CFU 0xA0
        if (len == ENTER_CFU_MODE_DATA_LEN)
        {
            if (recv[0] == 0x7e && recv[1] == 0x85)
            {
                matter_ble_send_msg(MATTER_BLE_MSG_ENTER_CFU, 0);
            }
        }
#endif

        for (uint32_t i = 0; i < len; i++)
        {
            rx_buffer[rx_tail] = recv[i];
            rx_tail = (rx_tail + 1) % kReceiveBufferSize;
        }
#if (ENABLE_PW_RPC != 1)
        BEE_EventSend(UART_RX, 0);
        otTaskletsSignalPending(NULL);
#endif
    }
}

void app_usb_spd_cb(uint8_t speed)
{
    DBG_DIRECT("[app_usb_spd_cb] speed = %d", speed);

    usb_speed_mode = speed;

    uint16_t out_mtu = 512;

    if (usb_speed_mode == USB_SPEED_FULL)
    {
        out_mtu = 0x40;
    }

    T_USB_CDC_DRIVER_ATTR out_attr =
    {
        .zlp = 1,
        .high_throughput = 0,
        .congestion_ctrl = CDC_DRIVER_CONGESTION_CTRL_DROP_CUR,
        .rsv = 0,
        .mtu = out_mtu
    };

    T_USB_CDC_DRIVER_ATTR in_attr =
    {
        .zlp = 1,
        .high_throughput = 0,
        .congestion_ctrl = CDC_DRIVER_CONGESTION_CTRL_DROP_CUR,
        .rsv = 0,
        .mtu = 512
    };

    if (!cdc_in_handle)
    {
        cdc_in_handle = usb_cdc_driver_data_pipe_open(CDC_BULK_IN_EP, in_attr, 4, CdcSendDataCb);
    }

    if (!cdc_out_handle)
    {
        cdc_out_handle = usb_cdc_driver_data_pipe_open(CDC_BULK_OUT_EP, out_attr, 2, CdcRecvDataCb);
    }
}

static char hex_str[29] = {0};
void convert_euid_to_hex_string()
{
    const uint8_t *euid_ptr = get_ic_euid();
    for (int i = 0; i < 14; i++)
    {
        snprintf(&hex_str[i * 2], 3, "%02x", euid_ptr[i]);
    }
    hex_str[28] = '\0';
    dev_strings[2].s = hex_str;
}

void uart_init_internal(void)
{
    convert_euid_to_hex_string();

    os_mutex_create(&uart_tx_mutex);
    os_sem_create(&uart_tx_sem, "cdc_tx", 0, 1);

    usb_spd_cb_register(app_usb_spd_cb);

    T_USB_CORE_CONFIG config = {.speed = usb_speed_mode, .class_set = {.hid_enable = 0, .uac_enable = 0}};

    usb_dm_core_init(config);

    usb_dev_driver_dev_desc_register((void *)&usb_dev_desc);
    usb_dev_driver_cfg_desc_register((void *)&usb_cfg_desc);
    usb_dev_driver_string_desc_register((void *)dev_stringtabs);

    void *inst0 = usb_cdc_driver_inst_alloc();
    usb_cdc_driver_if_desc_register(inst0, (void *)cdc_if0_descs_hs, (void *)cdc_if0_descs_fs);
    void *inst1 = usb_cdc_driver_inst_alloc();
    usb_cdc_driver_if_desc_register(inst1, (void *)cdc_if1_descs_hs, (void *)cdc_if1_descs_fs);

    T_LINE_CODING LineCoding = {.dwDTERate = 921600, .bCharFormat = 0, .bParityType = 0, .bDataBits = 8 };
    usb_cdc_driver_linecoding(inst0, LineCoding);

    T_USB_CDC_DRIVER_CBS cbs = {NULL};
    cbs.pfnGetLineCondigCb = GetLineCondigCb;
    cbs.pfnSetLineCondigCb = SetLineCondigCb;
    cbs.pfnSetControlLineStateCb = SetControlLineStateCb;
    usb_cdc_driver_cbs_register(inst0, &cbs);

    usb_cdc_driver_init();

    DBG_DIRECT("uart_init_internal: success");
}

extern void usb_isr_set_priority(uint8_t priority);
void usb_start(void)
{
    usb_isr_set_priority(2);
    usb_dm_start(false);
}

otError otPlatUartEnable(void)
{
    usb_start();

    return OT_ERROR_NONE;
}

otError otPlatUartDisable(void)
{
    DBG_DIRECT("Usb_Stop");

    usb_dm_stop();

    if (cdc_in_handle != NULL)
    {
        usb_cdc_driver_data_pipe_close(cdc_in_handle);
        cdc_in_handle = NULL;
    }

    if (cdc_out_handle != NULL)
    {
        usb_cdc_driver_data_pipe_close(cdc_out_handle);
        cdc_out_handle = NULL;
    }

    return OT_ERROR_NONE;
}

#define USB_UART_TX_BU_SZ 1024
char usb_uart_tx_buf[USB_UART_TX_BU_SZ] __ALIGNED(4);

void uart_send(const uint8_t *aBuf, uint16_t aBufLength)
{
    uint16_t out_mtu_size = (usb_speed_mode == USB_SPEED_FULL) ? 64 : 512;
    if (usb_uart_ready)
    {
        os_mutex_take(uart_tx_mutex, 0xffffffff);
        sRemainLength = aBufLength;
        sTransmitLength = 0;
        while (sRemainLength > 0)
        {
            if (sRemainLength > out_mtu_size)
            {
                mac_memcpy(usb_uart_tx_buf, &aBuf[sTransmitLength], out_mtu_size);
                usb_cdc_driver_data_pipe_send(cdc_in_handle, usb_uart_tx_buf, out_mtu_size);
                os_sem_take(uart_tx_sem, 0xffffffff);
                sTransmitLength += out_mtu_size;
                sRemainLength -= out_mtu_size;
            }
            else
            {
                mac_memcpy(usb_uart_tx_buf, &aBuf[sTransmitLength], sRemainLength);
                usb_cdc_driver_data_pipe_send(cdc_in_handle, usb_uart_tx_buf, sRemainLength);
                os_sem_take(uart_tx_sem, 0xffffffff);
                sTransmitLength += sRemainLength;
                sRemainLength = 0;
            }
        }
        os_mutex_give(uart_tx_mutex);
    }
}

otError otPlatUartSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    uart_send(aBuf, aBufLength);
    sTransmitDone = true;
    BEE_EventSend(UART_TX, 0);
    otTaskletsSignalPending(NULL);
    return OT_ERROR_NONE;
}

#if (FEATURE_SUPPORT_CFU && (USE_USB_CDC == USB_TYPE))
bool app_usb_send_dfu_data(uint8_t report_id, uint8_t *data, uint16_t len)
{
    uint8_t *send_buf = NULL;
    bool ret = false;


    os_mutex_take(uart_tx_mutex, 0xffffffff);
    send_buf = malloc(len + 1);
    if (!send_buf)
    {
        APP_PRINT_ERROR0("[app_usb_send_dfu_data] no memory for data");
        return ret;
    }
    send_buf[0] = report_id;
    memcpy(&send_buf[1], data, len);

    usb_cdc_driver_data_pipe_send(cdc_in_handle, send_buf, len + 1);
    os_sem_take(uart_tx_sem, 0xffffffff);
    os_mutex_give(uart_tx_mutex);

    free(send_buf);

    return ret;
}
#endif

#if (ENABLE_PW_RPC == 1)
extern int bee_putchar(const uint8_t *aBuf)
{
    if (usb_uart_ready)
    {
        mac_memcpy(usb_uart_tx_buf, aBuf, 1);
        usb_cdc_driver_data_pipe_send(cdc_in_handle, usb_uart_tx_buf, 1);
        os_sem_take(uart_tx_sem, 0xffffffff);
    }
    return OT_ERROR_NONE;
}

extern int bee_getchar(uint8_t *read_byte)
{
    if (rx_head == rx_tail)
    {
        return OT_ERROR_NONE;
    }

    *read_byte = rx_buffer[rx_head];
    rx_head = (rx_head + 1) % kReceiveBufferSize;

    return OT_ERROR_NONE;
}

void BEE_UartTx(void)
{
}

void BEE_UartRx(void)
{
}

#else // (ENABLE_PW_RPC == 1)

void BEE_UartTx(void)
{
    if (sTransmitDone)
    {
        sTransmitDone = false;
        otPlatUartSendDone();
    }
}

void BEE_UartRx(void)
{
    uint16_t tail;
    do
    {
        tail = rx_tail;
        if (rx_head > tail)
        {
            otPlatUartReceived(&rx_buffer[rx_head], kReceiveBufferSize - rx_head);
            rx_head = 0;
        }

        if (tail > rx_head)
        {
            otPlatUartReceived(&rx_buffer[rx_head], tail - rx_head);
            rx_head = tail;
        }
    }
    while (0);
}
#endif // (ENABLE_PW_RPC == 1)

#else // BUILD_USB

#if defined(DLPS_EN) && (DLPS_EN == 1)
extern void io_uart_dlps_allow_enter(void);
#endif

#define UART_TX_GDMA_CHANNEL_NUM        GDMA_CH_NUM4
#define UART_TX_GDMA_CHANNEL            GDMA_Channel4
#define UART_TX_GDMA_CHANNEL_IRQN       GDMA_Channel4_IRQn
#define UART_TX_GDMA_CHANNEL_VECTORN    GDMA0_Channel4_VECTORn

static volatile uint16_t sTransmitLength = 0;

void OT_UARTIntHandler(void);
void UART_TX_GDMA_Handler(void);

void uart_init_internal(void)
{
    os_mutex_create(&uart_tx_mutex);
    os_sem_create(&uart_tx_sem, "cdc_tx", 0, 1);

    NVIC_InitTypeDef NVIC_InitStruct;

    RCC_PeriphClockCmd(OT_UART_APB, OT_UART_APBCLK, ENABLE);
    /*--------------UART init-----------------------------*/
    UART_InitTypeDef UART_InitStruct;
    UART_StructInit(&UART_InitStruct);
#if (ENABLE_PW_RPC == 1)
    // Baud rate = 115200
    UART_InitStruct.div = 20;
    UART_InitStruct.ovsr = 12;
    UART_InitStruct.ovsr_adj = 0x252;
#else
#ifdef BOARD_RTL8771GTV
    switch (config_param.baud_rate)
    {
    case RCP_BAUDRATE_115200:
        UART_InitStruct.div = 0x38;
        UART_InitStruct.ovsr = 1;
        UART_InitStruct.ovsr_adj = 3;
        break;

    case RCP_BAUDRATE_230400:
        UART_InitStruct.div = 0x1C;
        UART_InitStruct.ovsr = 1;
        UART_InitStruct.ovsr_adj = 3;
        break;

    case RCP_BAUDRATE_460800:
        UART_InitStruct.div = 0xE;
        UART_InitStruct.ovsr = 1;
        UART_InitStruct.ovsr_adj = 3;
        break;

    case RCP_BAUDRATE_921600:
        UART_InitStruct.div = 7;
        UART_InitStruct.ovsr = 1;
        UART_InitStruct.ovsr_adj = 3;
        break;

    case RCP_BAUDRATE_1000000:
        UART_InitStruct.div = 8;
        UART_InitStruct.ovsr = 0;
        UART_InitStruct.ovsr_adj = 0;
        break;

    case RCP_BAUDRATE_2000000:
        UART_InitStruct.div = 4;
        UART_InitStruct.ovsr = 0;
        UART_InitStruct.ovsr_adj = 0;
        break;

    default:
        if ((config_param.func_msk & (1 << UART_TRX_LOG_OFFSET)) == 0x00)
        {
            DBG_DIRECT("baurdate %d not support, set to 921600", config_param.baud_rate);
        }
        UART_InitStruct.div = 7;
        UART_InitStruct.ovsr = 1;
        UART_InitStruct.ovsr_adj = 3;
        break;
    }
#else
    // Baud rate = 921600
    UART_InitStruct.div = 7;
    UART_InitStruct.ovsr = 1;
    UART_InitStruct.ovsr_adj = 3;
#endif
#endif

#ifdef BOARD_RTL8771GTV
    if (((config_param.func_msk & (1 << HW_FLOWCTL_OFFSET)) == 0x00) ||
        ((config_param.func_msk & (1 << FORCE_HW_FLOWCTL_OFFSET)) == 0x00))
    {
        DBG_DIRECT("enable flow control");
        UART_InitStruct.UART_HardwareFlowControl   = UART_HW_FLOW_CTRL_ENABLE;
    }
#endif
#if defined(DLPS_EN) && (DLPS_EN == 1)
    UART_InitStruct.rxTriggerLevel = 1;
    UART_InitStruct.UART_IdleTime       = UART_RX_IDLE_1BYTE;      //idle interrupt wait time
    UART_Init(OT_UART, &UART_InitStruct);

    UART_INTConfig(OT_UART, UART_INT_RD_AVA, ENABLE);
    NVIC_InitStruct.NVIC_IRQChannel = OT_UART_IRQN;
    NVIC_InitStruct.NVIC_IRQChannelCmd = (FunctionalState)ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_Init(&NVIC_InitStruct);
    RamVectorTableUpdate_ext(OT_UART_VECTORn, OT_UARTIntHandler);
#else
    UART_InitStruct.rxTriggerLevel = 16;
    UART_InitStruct.UART_IdleTime       = UART_RX_IDLE_1BYTE;      //idle interrupt wait time
    UART_InitStruct.UART_DmaEn          = UART_DMA_ENABLE;
    UART_InitStruct.UART_TxWaterLevel   = 15; /* Better to equal: TX_FIFO_SIZE - GDMA_MSize */
    UART_InitStruct.UART_RxWaterLevel   = 1;  /* Better to equal: GDMA_MSize */
    UART_InitStruct.UART_TxDmaEn        = ENABLE;
    UART_Init(OT_UART, &UART_InitStruct);

    UART_INTConfig(OT_UART, UART_INT_RD_AVA, ENABLE);
    UART_INTConfig(OT_UART, UART_INT_RX_IDLE, ENABLE);
    NVIC_InitStruct.NVIC_IRQChannel = OT_UART_IRQN;
    NVIC_InitStruct.NVIC_IRQChannelCmd = (FunctionalState)ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_Init(&NVIC_InitStruct);
    RamVectorTableUpdate_ext(OT_UART_VECTORn, OT_UARTIntHandler);

    RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);
    /*--------------GDMA init-----------------------------*/
    GDMA_InitTypeDef GDMA_InitStruct;
    GDMA_StructInit(&GDMA_InitStruct);
    GDMA_InitStruct.GDMA_ChannelNum       = UART_TX_GDMA_CHANNEL_NUM;
    GDMA_InitStruct.GDMA_DIR              = GDMA_DIR_MemoryToPeripheral;
    GDMA_InitStruct.GDMA_BufferSize       = 256;
    GDMA_InitStruct.GDMA_SourceInc        = DMA_SourceInc_Inc;
    GDMA_InitStruct.GDMA_DestinationInc   = DMA_DestinationInc_Fix;
    GDMA_InitStruct.GDMA_SourceDataSize   = GDMA_DataSize_Byte;
    GDMA_InitStruct.GDMA_DestinationDataSize = GDMA_DataSize_Byte;
    GDMA_InitStruct.GDMA_SourceMsize      = GDMA_Msize_1;
    GDMA_InitStruct.GDMA_DestinationMsize = GDMA_Msize_1;
    GDMA_InitStruct.GDMA_SourceAddr       = 0;
    GDMA_InitStruct.GDMA_DestinationAddr  = (uint32_t)(&(OT_UART->UART_RBR_THR));
    GDMA_InitStruct.GDMA_DestHandshake    = GDMA_Handshake_UART3_TX;
    GDMA_InitStruct.GDMA_ChannelPriority  = 2;
    GDMA_Init(UART_TX_GDMA_CHANNEL, &GDMA_InitStruct);

    /*-----------------GDMA IRQ init-------------------*/
    GDMA_INTConfig(UART_TX_GDMA_CHANNEL_NUM, GDMA_INT_Transfer, ENABLE);
    NVIC_InitStruct.NVIC_IRQChannel = UART_TX_GDMA_CHANNEL_IRQN;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
    RamVectorTableUpdate_ext(UART_TX_GDMA_CHANNEL_VECTORN, UART_TX_GDMA_Handler);
#endif
}

otError otPlatUartEnable(void)
{
    return OT_ERROR_NONE;
}

otError otPlatUartDisable(void)
{
    UART_DeInit(OT_UART);
    return OT_ERROR_NONE;
}

void uart_send(const uint8_t *aBuf, uint16_t aBufLength)
{
#if defined(DLPS_EN) && (DLPS_EN == 1)
    for (uint16_t i = 0; i < aBufLength; i++)
    {
        UART_SendByte(OT_UART, aBuf[i]);
        while (UART_GetFlagStatus(OT_UART, UART_FLAG_TX_EMPTY) != SET);
    }
#else
    GDMA_SetSourceAddress(UART_TX_GDMA_CHANNEL, (uint32_t)aBuf);
    GDMA_SetDestinationAddress(UART_TX_GDMA_CHANNEL, (uint32_t) & (OT_UART->UART_RBR_THR));
    GDMA_SetBufferSize(UART_TX_GDMA_CHANNEL, aBufLength);
    GDMA_Cmd(UART_TX_GDMA_CHANNEL_NUM, ENABLE);
    os_sem_take(uart_tx_sem, 0xffffffff);
#endif
}

otError otPlatUartSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    uart_send(aBuf, aBufLength);
    otPlatUartSendDone();
#if defined(DLPS_EN) && (DLPS_EN == 1)
    io_uart_dlps_allow_enter();
#endif
    return OT_ERROR_NONE;
}

#if (ENABLE_PW_RPC == 1)
extern int bee_putchar(const uint8_t *aBuf)
{
    GDMA_SetSourceAddress(UART_TX_GDMA_CHANNEL, (uint32_t)aBuf);
    GDMA_SetDestinationAddress(UART_TX_GDMA_CHANNEL, (uint32_t) & (OT_UART->UART_RBR_THR));
    GDMA_SetBufferSize(UART_TX_GDMA_CHANNEL, 1);
    GDMA_Cmd(UART_TX_GDMA_CHANNEL_NUM, ENABLE);
    os_sem_take(uart_tx_sem, 0xffffffff);

    return OT_ERROR_NONE;
}

extern int bee_getchar(uint8_t *read_byte)
{
    if (rx_head == rx_tail)
    {
        return OT_ERROR_NONE;
    }

    *read_byte = rx_buffer[rx_head];
    rx_head = (rx_head + 1) % kReceiveBufferSize;

    DBG_DIRECT("bee_getchar %c", *read_byte);

    return OT_ERROR_NONE;
}

void BEE_UartTx(void)
{
}

void BEE_UartRx(void)
{
}
#else // (ENABLE_PW_RPC == 1)
void BEE_UartTx(void)
{
}

void BEE_UartRx(void)
{
    uint16_t tail = curr_rx_tail;
    if (rx_head != tail)
    {
        if (rx_head > tail)
        {
            otPlatUartReceived(&rx_buffer[rx_head], kReceiveBufferSize - rx_head);
            rx_head = 0;
        }

        if (tail > rx_head)
        {
            otPlatUartReceived(&rx_buffer[rx_head], tail - rx_head);
            rx_head = tail;
        }
#if defined(DLPS_EN) && (DLPS_EN == 1)
        io_uart_dlps_allow_enter();
#endif
    }
}
#endif // (ENABLE_PW_RPC == 1)

APP_RAM_TEXT_SECTION void UART_TX_GDMA_Handler(void)
{
    GDMA_ClearINTPendingBit(UART_TX_GDMA_CHANNEL_NUM, GDMA_INT_Transfer);
    os_sem_give(uart_tx_sem);
}

APP_RAM_TEXT_SECTION void OT_UARTIntHandler(void)
{
#if defined(DLPS_EN) && (DLPS_EN == 1)
    uint32_t int_status = UART_GetIID(OT_UART);
    uint16_t len;

    UART_INTConfig(OT_UART, UART_INT_RD_AVA, DISABLE);
    switch (int_status & 0x0E)
    {
    case UART_INT_ID_RX_LEVEL_REACH:
    case UART_INT_ID_RX_DATA_TIMEOUT:
        len = UART_GetRxFIFODataLen(OT_UART);
        for (uint16_t i = 0; i < len; i++)
        {
            rx_buffer[rx_tail] = OT_UART->UART_RBR_THR;
            rx_tail = (rx_tail + 1) % kReceiveBufferSize;
        }
        curr_rx_tail = rx_tail;
        BEE_EventSend(UART_RX, 0);
        otSysEventSignalPending();
        break;

    default:
        break;
    }

    UART_INTConfig(OT_UART, UART_INT_RD_AVA, ENABLE);
#else
    uint32_t int_status = UART_GetIID(OT_UART);
    uint16_t len;

    UART_INTConfig(OT_UART, UART_INT_RD_AVA, DISABLE);
#if 1
    if (UART_GetFlagStatus(OT_UART, UART_FLAG_RX_IDLE))
    {
        UART_INTConfig(OT_UART, UART_INT_RX_IDLE, DISABLE);
        len = UART_GetRxFIFODataLen(OT_UART);
        for (uint16_t i = 0; i < len; i++)
        {
            rx_buffer[rx_tail] = OT_UART->UART_RBR_THR;
            rx_tail = (rx_tail + 1) % kReceiveBufferSize;
        }
        UART_INTConfig(OT_UART, UART_INT_RX_IDLE, ENABLE);
#if (ENABLE_PW_RPC != 1)
        curr_rx_tail = rx_tail;
        BEE_EventSend(UART_RX, 0);
        otSysEventSignalPending();
#endif
    }
#endif
    switch (int_status & 0x0E)
    {
    case UART_INT_ID_RX_LEVEL_REACH:
    case UART_INT_ID_RX_DATA_TIMEOUT:
        len = UART_GetRxFIFODataLen(OT_UART);
        for (uint16_t i = 0; i < len; i++)
        {
            rx_buffer[rx_tail] = OT_UART->UART_RBR_THR;
            rx_tail = (rx_tail + 1) % kReceiveBufferSize;
        }
        break;

    default:
        break;
    }

    UART_INTConfig(OT_UART, UART_INT_RD_AVA, ENABLE);
#endif
}
#endif // BUILD_USB

otError otPlatUartFlush(void)
{
    return OT_ERROR_NONE;
}

#else // (ENABLE_CLI == 1 || BUILD_NCP == 1 || BUILD_RCP == 1 || ENABLE_PW_RPC == 1)

void uart_init_internal(void)
{

}

void uart_send(const uint8_t *aBuf, uint16_t aBufLength)
{
}

otError otPlatUartSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    return OT_ERROR_NONE;
}

void BEE_UartTx(void)
{
}

void BEE_UartRx(void)
{
}

#endif // (ENABLE_CLI == 1 || BUILD_NCP == 1 || BUILD_RCP == 1 || ENABLE_PW_RPC == 1)
