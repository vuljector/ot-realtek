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
 *   This file implements the OpenThread platform abstraction for radio communication.
 *
 */
#include "platform-bee.h"
#include "mac_driver.h"
#include "mac_driver_mpan.h"
#include "mac_data_type.h"
#include "mac_stats.h"

#include <openthread-core-config.h>
#include <openthread/config.h>
#include <openthread/tasklet.h>
#include <openthread/message.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <openthread/thread.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/code_utils.h"
#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
#include "utils/link_metrics.h"
#endif
#include "utils/mac_frame.h"
#include "utils/uart.h"
//#include <platform-config.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/alarm-micro.h>
#include <openthread/platform/diag.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/time.h>

#include "openthread-system.h"

#include <openthread/random_noncrypto.h>
#include "dbg_printf.h"

// clang-format off
#define FCS_LEN               2

#define SHORT_ADDRESS_SIZE    2            ///< Size of MAC short address.
#define US_PER_MS             1000ULL      ///< Microseconds in millisecond.

#define ACK_REQUEST_OFFSET       1         ///< Byte containing Ack request bit (+1 for frame length byte).
#define ACK_REQUEST_BIT          (1 << 5)  ///< Ack request bit.
#define FRAME_PENDING_OFFSET     1         ///< Byte containing pending bit (+1 for frame length byte).
#define FRAME_PENDING_BIT        (1 << 4)  ///< Frame Pending bit.
#define SECURITY_ENABLED_OFFSET  1         ///< Byte containing security enabled bit (+1 for frame length byte).
#define SECURITY_ENABLED_BIT     (1 << 3)  ///< Security enabled bit.

#define RSSI_SETTLE_TIME_US   40           ///< RSSI settle time in microseconds.
#define SAFE_DELTA            1000         ///< A safe value for the `dt` parameter of delayed operations.

#define CSL_UNCERT            74           ///< The Uncertainty of the scheduling CSL of transmission by the parent, in ±10 us units.
#define PHY_SYMBOL_TIME       16           ///< 1 symbol time in us.
#define SHR_DURATION_US       160          ///< Duration of SHR in us, 10 * PHY_SYMBOL_TIME.
#define PHR_DURATION_US       32           ///< Duration of PHR in us.
#define IMMACK_DURATION_US    352          ///< 5B SRHR + 1B PHR + 2B MHR + 1B SEQ + 2B FCS = 11 Bytes * 2 Symbol * 16 us = 352 us
#define EXTRA_PROTECTION_EN   0
#define TRANSMIT_RETRIES_EN   1

#define TX_BACKOFF_USE_MS_ALARM 0
#define TX_BACKOFF_USE_US_ALARM 1
#define TX_BACKOFF_USE_MAC_TMR 2

#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
#define TX_BACKOFF_TMR_MODE TX_BACKOFF_USE_MS_ALARM
#else
#define TX_BACKOFF_TMR_MODE TX_BACKOFF_USE_US_ALARM
#endif

const uint8_t kTxBackoffTmrMode = TX_BACKOFF_TMR_MODE;
extern void mac_TrigMacTmr(uint32_t period_us);
#if defined(__ICCARM__)
_Pragma("diag_suppress=Pe167")
#endif

enum
{
    SBEE2_RECEIVE_SENSITIVITY  = -100, // dBm
    SBEE2_MIN_CCA_ED_THRESHOLD = -94,  // dBm
};

// clang-format on
#define IEEE802154_MAX_LENGTH    (127)
#define MAC_FRAME_TX_HDR_LEN     (2)     // extra length of frame header for ASIC depend TX frame description
#define MAC_FRAME_RX_HDR_LEN     (1)     // extra length of rx frame header: frame length
#define MAC_FRAME_RX_TAIL_LEN    (7)     // extra length of frame trailer: LQI + RSSI
#define MAC_FRAME_IMMACK_LEN     (3)

#define TX_NONE 0
#define TX_WAIT_ACK 1
#define TX_TERMED 2
#define TX_OK 3
#define TX_CCA_FAIL 4
#define TX_NO_ACK 5
#define TX_PTA_FAIL 6
#define TX_AT_FAIL 7
#define TX_BACKOFF_DONE 8

#define DEFAULT_CCA_ED_THRES 20
#define DEFAULT_CCA_CS_THRES 25
#define MAX_TRANSMIT_RETRY 16
#define MAX_TRANSMIT_CCAFAIL 16
#ifdef BUILD_MATTER
#define MAX_TRANSMIT_BC_RETRY 1 // 1 times retry for broadcast frame
#else
#define MAX_TRANSMIT_BC_RETRY 0
#endif

#if defined(DLPS_EN) && (DLPS_EN == 1)
#define WAIT_ACK_TX_DONE_EN 1
#else
#define WAIT_ACK_TX_DONE_EN 1
#endif
#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
extern bool mac_SoftwareTimer_IsRunning(pmac_timer_handle_t pmac_tmr);
#endif

extern volatile uint64_t micro_alarm_us[MAX_PAN_NUM];
extern volatile uint64_t milli_alarm_us[MAX_PAN_NUM];
APP_RAM_TEXT_SECTION void handle_tx_backoff_pending(uint8_t pan_idx);

typedef struct
{
    otRadioFrame sReceivedFrames;
    uint8_t sReceivedPsdu[MAC_FRAME_RX_HDR_LEN + IEEE802154_MAX_LENGTH + MAC_FRAME_RX_TAIL_LEN];
    uint8_t sReceivedChannel;
} rx_item_t;

typedef struct
{
    union
    {
        volatile uint8_t sStayAwake;

        struct
        {
            volatile uint8_t waitAckTxDone           : 1;  // waiting until ACK transmission is done
            volatile uint8_t waitTxDone              : 1;  // waiting until transmission is done
            volatile uint8_t waitRxPendingData       : 1;  // waiting for pending data to be received
            volatile uint8_t waitScheduledRxWindow   : 1;  // waiting for a scheduled receive window
            volatile uint8_t rxOnWhenIdle            : 1;  // keeping the receiver on when the system is idle
            volatile uint8_t reserved                : 3;  // reserved bit field for future use
        } sStayAwake_b;
    };

#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
    mac_timer_handle_t *sWaitAckTxDoneTimer;
#endif

#if defined(DLPS_EN) && (DLPS_EN == 1)
    mac_timer_handle_t *sWaitRxPendingDataTimer;
#endif
    volatile uint64_t dbg_exit_dlps;
    bool sDisabled;
    otRadioState sState;
    uint8_t curr_channel;
    uint16_t sPanid;
    int8_t sPower;

    volatile uint16_t rx_tail;
    volatile uint16_t rx_head;
    rx_item_t rx_queue[RX_BUF_SIZE];
    rx_item_t ack_item;
#if defined(RT_PLATFORM_BEE3PLUS)
    rx_item_t ack_received;
#endif
    bool ack_fp;
    bool ack_receive_done;

    volatile bool tx_backoff_tmo;
    volatile uint64_t tx_backoff_pending;
    uint32_t tx_backoff_delay;
    uint8_t tx_result;
    void *radio_done;

    uint32_t     sTransmitRetry;
    uint32_t     sTransmitCcaFailCnt;
    otRadioFrame sTransmitFrame;
    uint8_t      sTransmitPsdu[MAC_FRAME_TX_HDR_LEN + IEEE802154_MAX_LENGTH];

#if OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT
    otRadioIeInfo sTransmitIeInfo;
#endif

// tx enhack frame
    uint8_t sEnhAckPsdu[MAC_FRAME_TX_HDR_LEN + IEEE802154_MAX_LENGTH];
    uint8_t enhack_frm_len;
    bool    enhack_frm_sec_en;

    int8_t   sLnaGain;
    uint16_t sRegionCode;

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    uint32_t      sCslPeriod;
    uint32_t      sCslSampleTime;
    uint8_t       sCslIeIndex;
#endif // OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
    uint32_t      sCstPeriod; // us
    uint64_t      sCstSampleTime;
    uint8_t       sCstMaxRetry;
#endif // OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    uint8_t       enhAckProbingDataLen;
    uint8_t       sVendorIeIndex;
#endif

    uint32_t sEnergyDetectionTime;
    uint8_t sEnergyDetectionChannel;

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    uint32_t        sMacFrameCounter;
    uint8_t         sKeyId;
    otMacKeyMaterial sPrevKey;
    otMacKeyMaterial sCurrKey;
    otMacKeyMaterial sNextKey;
#endif
} radio_inst_t;
static radio_inst_t radio_inst[MAX_PAN_NUM];

typedef struct _fc_t
{
    uint16_t type: 3;
    uint16_t sec_en: 1;
    uint16_t pending: 1;
    uint16_t ack_req: 1;
    uint16_t panid_compress: 1;
    uint16_t rsv: 1;
    uint16_t seq_num_suppress: 1;
    uint16_t ie_present: 1;
    uint16_t dst_addr_mode: 2;
    uint16_t ver: 2;
    uint16_t src_addr_mode: 2;
} fc_t;

#define FRAME_TYPE_BEACON   0
#define FRAME_TYPE_DATA     1
#define FRAME_TYPE_ACK      2
#define FRAME_TYPE_COMMAND  3

#define ADDR_MODE_NOT_PRESENT   0
#define ADDR_MODE_RSV           1
#define ADDR_MODE_SHORT         2
#define ADDR_MODE_EXTEND        3

#define FRAME_VER_2003 0
#define FRAME_VER_2006 1
#define FRAME_VER_2015 2

#define SEC_NONE        0
#define SEC_MIC_32      1
#define SEC_MIC_64      2
#define SEC_MIC_128     3
#define SEC_ENC         4
#define SEC_ENC_MIC_32  5
#define SEC_ENC_MIC_64  6
#define SEC_ENC_MIC_128 7

#define KEY_ID_MODE_0  0
#define KEY_ID_MODE_1  1
#define KEY_ID_MODE_2  2
#define KEY_ID_MODE_3  3

typedef struct _aux_sec_ctl_t
{
    uint8_t sec_level: 3;
    uint8_t key_id_mode: 2;
    uint8_t frame_counter_supp: 1;
    uint8_t asn_in_nonce: 1;
    uint8_t rsv: 1;
} aux_sec_ctl_t;

typedef struct
{
    uint16_t len: 7;
    uint16_t id: 8;
    uint16_t type: 1;
    uint16_t phase;
    uint16_t period;
} __attribute__((packed)) csl_ie_t;

typedef struct
{
    uint16_t len: 7;
    uint16_t id: 8;
    uint16_t type: 1;
    uint8_t oui[3];
    uint8_t subtype;
    uint8_t content[2];
} __attribute__((packed)) vendor_ie_t;

#ifdef _IS_FPGA_
#else
#ifdef RT_PLATFORM_BEE3PLUS
extern bool (*hw_sha256)(uint8_t *input, uint32_t byte_len, uint32_t *result, int mode);
#else
extern bool hw_sha256(uint8_t *input, uint32_t byte_len, uint32_t *result, int mode);
#endif
static uint8_t sha256_output[32];
#endif

#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
extern void (*zbmac_pm_init)(zbpm_adapter_t *padapter);
static zbpm_adapter_t zbpm_adap;
static bool zbpm_inited = false;
#endif

void pm_check_inactive(const char *callerStr, bool wakeup)
{
#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
    volatile zbpm_adapter_t *padapter = &zbpm_adap;
    extern bool zbmac_pm_check_active(void);
    extern void zbmac_pm_initiate_wakeup(void);

    if (zbpm_inited == false)
    {
        return;
    }

    if (zbmac_pm_check_active() == false)
    {
        otLogWarnPlat("%s zbmac_pm_check_inactive power_mode %lu", callerStr, padapter->power_mode);
        if (wakeup == true)
        {
            zbmac_pm_initiate_wakeup();
        }
    }
#endif
}

static void pm_init(void)
{
#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
    zbpm_adapter_t *padapter = &zbpm_adap;

    if (true == zbpm_inited)
    {
        return;
    }

    zbpm_inited = true;
    padapter->power_mode = ZBMAC_ACTIVE;
    padapter->wakeup_reason = ZBMAC_PM_WAKEUP_UNKNOWN;
    padapter->error_code = ZBMAC_PM_ERROR_UNKNOWN;

    padapter->stage_time[ZBMAC_PM_CHECK] = 20;
    padapter->stage_time[ZBMAC_PM_STORE] = 15;
    padapter->stage_time[ZBMAC_PM_ENTER] = 5;
    padapter->stage_time[ZBMAC_PM_EXIT] = 5;
    padapter->stage_time[ZBMAC_PM_RESTORE] = 20;
    padapter->minimum_sleep_time = 20;
    padapter->learning_guard_time = 7; // 3 (learning guard time) + 4 (two 16k po_intr drift)

    padapter->cfg.wake_interval_en = 0;
    padapter->cfg.stage_time_learned = 0;
    padapter->wakeup_time_us = 0;
    padapter->wakeup_interval_us = 0;
    padapter->enter_callback = NULL;
    padapter->exit_callback = NULL;

    otLogNotePlat("zbmac_pm_init");
    zbmac_pm_init(padapter);
#endif
}

#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
void waitAckTxDoneProtectionTimeout(void *arg)
{
    uint8_t pan_idx = (uint32_t)arg;

    if (radio_inst[pan_idx].sStayAwake_b.waitAckTxDone)
    {
        radio_inst[pan_idx].sStayAwake_b.waitAckTxDone = 0;
    }
}
#endif

void startWaitAckTxDoneProtection(uint8_t pan_idx, uint64_t ackEndTime, uint32_t guardTime)
{
#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
    uint64_t now = otPlatTimeGet();

    ackEndTime += guardTime;

    if (now >= ackEndTime) // already timeout
    {
        waitAckTxDoneProtectionTimeout((void *)(uint32_t)pan_idx);
        return;
    }

    if (radio_inst[pan_idx].sWaitAckTxDoneTimer)
    {
        uint32_t diff = ackEndTime - now;

        if (diff < 20)
        {
            diff = 20;
        }

        mac_SoftwareTimer_Start(radio_inst[pan_idx].sWaitAckTxDoneTimer,
                                mac_GetCurrentBTUS() + diff,
                                waitAckTxDoneProtectionTimeout,
                                (void *)(uint32_t)pan_idx);
    }
    //radio_inst[pan_idx].sWaitAckTxDoneTimestamp = ackEndTime;
#endif
}

static void dataInit(uint8_t pan_idx)
{
    os_sem_create(&radio_inst[pan_idx].radio_done, "radio_done", 0, 1);
    radio_inst[pan_idx].sDisabled = true;
    radio_inst[pan_idx].sStayAwake_b.rxOnWhenIdle = true;
    radio_inst[pan_idx].sTransmitFrame.mPsdu = &radio_inst[pan_idx].sTransmitPsdu[MAC_FRAME_TX_HDR_LEN];

#if OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT
    radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mIeInfo = &radio_inst[pan_idx].sTransmitIeInfo;
#endif

    pm_init();

#if defined(DLPS_EN) && (DLPS_EN == 1)
    radio_inst[pan_idx].sWaitRxPendingDataTimer = mac_SoftwareTimer_Alloc();
    if (!radio_inst[pan_idx].sWaitRxPendingDataTimer)
    {
        otLogWarnPlat("sWaitRxPendingDataTimer alloc fail");
    }
#endif

#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
    radio_inst[pan_idx].sWaitAckTxDoneTimer = mac_SoftwareTimer_Alloc();
    if (!radio_inst[pan_idx].sWaitAckTxDoneTimer)
    {
        otLogWarnPlat("sWaitAckTxDoneTimer alloc fail");
    }
#endif

#ifdef _IS_FPGA_
#else
    hw_sha256(get_ic_euid(), 14, sha256_output, 0);
#endif
}

static void dataDeinit(uint8_t pan_idx)
{
    otLogInfoPlat("%s %d", __func__, pan_idx);
#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
    if (radio_inst[pan_idx].sWaitAckTxDoneTimer)
    {
        mac_SoftwareTimer_Free(radio_inst[pan_idx].sWaitAckTxDoneTimer);
    }
#endif
    os_sem_delete(radio_inst[pan_idx].radio_done);
}

/* when the upper layer protocol stack enable or disable the MAC function,
   this callback function should be called to maintain power management state */
void __attribute__((weak)) mac_enable_ctrol_callback(uint8_t pan_idx, uint8_t isEnable)
{
    /*
        this is a dummy weak function to prevent compiler error,
        the workable implementation should be implemented in MAC driver
    */
    OT_UNUSED_VARIABLE(pan_idx);
    OT_UNUSED_VARIABLE(isEnable);
}

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
APP_RAM_TEXT_SECTION void txAckProcessSecurity(uint8_t *aAckFrame, uint8_t pan_idx)
{
    //otRadioFrame     ackFrame;
    otMacKeyMaterial *key = NULL;
    uint8_t          keyId;

    struct
    {
        uint8_t sec_level;
        uint32_t frame_counter;
        uint64_t src_ext_addr;
    } __attribute__((packed)) nonce;

    //otEXPECT(aAckFrame[SECURITY_ENABLED_OFFSET] & SECURITY_ENABLED_BIT);
    otEXPECT(mac_GetRxFrmSecEn());

    //memset(&ackFrame, 0, sizeof(ackFrame));
    //ackFrame.mPsdu   = &aAckFrame[1];
    //ackFrame.mLength = aAckFrame[0];

    //keyId = otMacFrameGetKeyId(&ackFrame);
    keyId = mac_GetRxFrmSecKeyId();

    //otEXPECT(otMacFrameIsKeyIdMode1(&ackFrame) && keyId != 0);
    otEXPECT(0x01 == mac_GetRxFrmSecKeyIdMode() && keyId != 0);

    if (keyId == radio_inst[pan_idx].sKeyId)
    {
        key = &radio_inst[pan_idx].sCurrKey;
    }
    else if (keyId == radio_inst[pan_idx].sKeyId - 1)
    {
        key = &radio_inst[pan_idx].sPrevKey;
    }
    else if (keyId == radio_inst[pan_idx].sKeyId + 1)
    {
        key = &radio_inst[pan_idx].sNextKey;
    }
    else
    {
        otEXPECT(false);
    }

    //ackFrame.mInfo.mTxInfo.mAesKey = key;

    //otMacFrameSetKeyId(&ackFrame, keyId);
    //otMacFrameSetFrameCounter(&ackFrame, sMacFrameCounter++);

    //otMacFrameProcessTransmitAesCcm(&ackFrame, &sExtAddress);
    nonce.sec_level = mac_GetRxFrmSecLevel();
    nonce.frame_counter = radio_inst[pan_idx].sMacFrameCounter++;
    mac_memcpy(&nonce.src_ext_addr, mpan_GetLongAddress(pan_idx), sizeof(nonce.src_ext_addr));

    // set nonce
    mac_LoadNonce((uint8_t *)&nonce);
    // set key
    mac_LoadTxEnhAckKey(key->mKeyMaterial.mKey.m8);
    // set security level
    mac_SetTxEnhAckCipher(nonce.sec_level);
exit:
    return;
}
#endif // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2

#if !OPENTHREAD_CONFIG_ENABLE_PLATFORM_EUI64_CUSTOM_SOURCE
void otPlatRadioGetIeeeEui64(otInstance *aInstance, uint8_t *aIeeeEui64)
{
#ifdef _IS_FPGA_
    aIeeeEui64[0] = 0x04; aIeeeEui64[1] = 0x0e; aIeeeEui64[2] = 0x0e; aIeeeEui64[3] = 0x0b;
    aIeeeEui64[4] = 0x00; aIeeeEui64[5] = 0x00; aIeeeEui64[6] = 0x55; aIeeeEui64[7] = 0xaa;
#else
    memcpy(aIeeeEui64, sha256_output, sizeof(uint64_t));
#endif
}

void otPlatRadioSetIeeeEui64(otInstance *aInstance, const uint8_t *aIeeeEui64)
{
    otLogInfoPlat("%02x%02x%02x%02x%02x%02x%02x%02x", aIeeeEui64[7], aIeeeEui64[6], aIeeeEui64[5],
                  aIeeeEui64[4],
                  aIeeeEui64[3], aIeeeEui64[2], aIeeeEui64[1], aIeeeEui64[0]);
}
#endif // OPENTHREAD_CONFIG_ENABLE_PLATFORM_EUI64_CUSTOM_SOURCE

uint64_t otPlatTimeGetWithBTUS(uint32_t *btus)
{
    uint64_t now;
    uint32_t s = os_lock();
    *btus = mac_GetCurrentBTUS();
    now = bt_clk_offset + *btus;
    os_unlock(s);
    return now;
}

void otPlatRadioSetPanId(otInstance *aInstance, uint16_t aPanid)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otLogInfoPlat("PANID=%X", aPanid);
    radio_inst[pan_idx].sPanid = aPanid;
    mpan_SetPANId(aPanid, pan_idx);
}

void otPlatRadioSetRxOnWhenIdle(otInstance *aInstance, bool aEnable)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    radio_inst[pan_idx].sStayAwake_b.rxOnWhenIdle = aEnable;
    otLogNotePlat("otPlatRadioSetRxOnWhenIdle %d", aEnable);
    BEE_SleepProcess(aInstance, pan_idx);
}

void otPlatRadioSetExtendedAddress(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otLogInfoPlat("ExtAddr=%X%X%X%X%X%X%X%X", aExtAddress->m8[7], aExtAddress->m8[6],
                  aExtAddress->m8[5], aExtAddress->m8[4],
                  aExtAddress->m8[3], aExtAddress->m8[2], aExtAddress->m8[1], aExtAddress->m8[0]);
    mpan_SetLongAddress((u8_t *)aExtAddress->m8, pan_idx);
}

void otPlatRadioSetShortAddress(otInstance *aInstance, uint16_t aShortAddress)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otLogInfoPlat("ShortAddr=%X", aShortAddress);
    mpan_SetShortAddress(aShortAddress, pan_idx);
}

#ifdef RT_PLATFORM_RTL8922D
extern void zb_modem_set_zb_cca_energy_detect_threshold(uint8_t thres, uint8_t res);
#else
extern void (*modem_set_zb_cca_energy_detect_threshold)(int8_t thres, uint8_t res);
#endif
extern mac_attribute_t attr;
extern mac_driver_t drv;

void txnterr_handler(uint8_t pan_idx, uint32_t arg);
void txn_handler(uint8_t pan_idx, uint32_t arg);
void rxely_handler(uint8_t pan_idx, uint32_t arg);
void rxdone_handler(uint8_t pan_idx, uint32_t arg);
void edscan_handler(uint8_t pan_idx, uint32_t arg);
void btcmp0_handler(uint8_t pan_idx, uint32_t arg);
void btcmp1_handler(uint8_t pan_idx, uint32_t arg);
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
void btcmp4_handler(uint8_t pan_idx, uint32_t arg);
void btcmp5_handler(uint8_t pan_idx, uint32_t arg);
#endif

void BEE_RadioInit(uint8_t pan_idx)
{
    otLogNotePlat("%s %d", __func__, pan_idx);
    mac_SetTxNCsma(false);
    mac_SetTxNMaxRetry(0);

    if (pan_idx == 0)
    {
        mpan_RegisterISR(0, txnterr_handler, txn_handler, rxely_handler, rxdone_handler, edscan_handler);
        mpan_RegisterTimer(0, MAC_BT_TIMER0, btcmp0_handler, 0);
        mpan_RegisterTimer(0, MAC_BT_TIMER1, btcmp1_handler, 0);
        mpan_EnableCtl(0, 1);
        dataInit(0);
    }
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
    else
    {
        mpan_RegisterISR(1, txnterr_handler, txn_handler, rxely_handler, rxdone_handler, edscan_handler);
        mpan_RegisterTimer(1, MAC_BT_TIMER4, btcmp4_handler, 0);
        mpan_RegisterTimer(1, MAC_BT_TIMER5, btcmp5_handler, 0);
        mpan_EnableCtl(1, 1);
        dataInit(1);
    }
#endif
#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
    mpan_SetAcceptFrm(1, 43, 0);
#endif
}

void BEE_RadioDeinit(uint8_t pan_idx)
{
    otLogNotePlat("%s %d", __func__, pan_idx);
    if (pan_idx == 0)
    {
        mpan_EnableCtl(0, 0);
        dataDeinit(0);
    }
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
    else
    {
        mpan_EnableCtl(1, 0);
        dataDeinit(1);
    }
#endif
}


otRadioState otPlatRadioGetState(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    if (radio_inst[pan_idx].sDisabled)
    {
        return OT_RADIO_STATE_DISABLED;
    }

    return radio_inst[pan_idx].sState; // It is the default state. Return it in case of unknown.
}

// the API for application (ex. coexustence coordinator) to get current radio state
uint8_t appPlatRadioGetState(uint8_t pan_idx)
{
    if ((MAX_PAN_NUM <= pan_idx) || (radio_inst[pan_idx].sDisabled))
    {
        return OT_RADIO_STATE_DISABLED;
    }

    return radio_inst[pan_idx].sState; // It is the default state. Return it in case of unknown.
}

bool otPlatRadioIsEnabled(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    return (radio_inst[pan_idx].sState != OT_RADIO_STATE_DISABLED) ? true : false;
}

otError otPlatRadioEnable(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    otLogNotePlat("otPlatRadioEnable");
    if (!otPlatRadioIsEnabled(aInstance))
    {
        // TODO: enable radio and enter sleep mode
        otLogInfoPlat("State=OT_RADIO_STATE_SLEEP");
        radio_inst[pan_idx].sState = OT_RADIO_STATE_SLEEP;
    }
    radio_inst[pan_idx].sDisabled = false;
    //mpan_mac_lock(pan_idx);
    //mac_enable_ctrol_callback(pan_idx, 1/*enable*/);
    //mpan_mac_unlock();

    return OT_ERROR_NONE;
}

otError otPlatRadioDisable(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    otLogNotePlat("otPlatRadioDisable");
    if (otPlatRadioIsEnabled(aInstance))
    {
        otLogInfoPlat("State=OT_RADIO_STATE_DISABLED");
        radio_inst[pan_idx].sDisabled = true;
        radio_inst[pan_idx].sState = OT_RADIO_STATE_DISABLED;
        // TODO: radio disable
        //mpan_mac_lock(pan_idx);
        //mac_enable_ctrol_callback(pan_idx, 0/* disable */);
        //mpan_mac_unlock();
    }

    return OT_ERROR_NONE;
}

otError otPlatRadioSleep(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otError error = OT_ERROR_INVALID_STATE;

    otLogInfoPlat("otPlatRadioSleep");
    if (radio_inst[pan_idx].sState != OT_RADIO_STATE_DISABLED)
    {
        error  = OT_ERROR_NONE;
        radio_inst[pan_idx].sState = OT_RADIO_STATE_SLEEP;
        // TODO: radio enter sleep
        BEE_SleepProcess(aInstance, pan_idx);
    }

    return error;
}

void setChannel(otRadioFrame *aTxFrame, uint8_t aChannel, uint8_t pan_idx)
{
    if (aChannel == 0)
    {
        return;
    }

    mac_RadioOn();
    if (radio_inst[pan_idx].curr_channel != aChannel)
    {
        otLogNotePlat("setChannel[%u] %u", pan_idx, aChannel);

#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
        // wait for tx ack transmit completed
        while (mac_SoftwareTimer_IsRunning(radio_inst[pan_idx].sWaitAckTxDoneTimer))
        {
            otLogWarnPlat("Warning: Attempt to change channel during pending ACK transmission");
        }
#endif

        mpan_mac_lock(pan_idx);
        if (aTxFrame)
        {
            aTxFrame->mInfo.mTxInfo.mRxChannelAfterTxDone = radio_inst[pan_idx].curr_channel;
        }
        radio_inst[pan_idx].curr_channel = aChannel;
        mpan_SetChannel(aChannel, pan_idx);
        mpan_mac_unlock();
    }
}

otError otPlatRadioReceive(otInstance *aInstance, uint8_t aChannel)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otError error = OT_ERROR_INVALID_STATE;

    if (radio_inst[pan_idx].sState != OT_RADIO_STATE_DISABLED)
    {
        error  = OT_ERROR_NONE;
        radio_inst[pan_idx].sState = OT_RADIO_STATE_RECEIVE;
#ifdef EXTRA_PROTECTION_EN
        pm_check_inactive("otPlatRadioReceive", true);
#endif
        setChannel(NULL, aChannel, pan_idx);
        // TODO: radio enter RX ready
        mpan_Rx(pan_idx);
    }
    return error;
}

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
otError otPlatRadioReceiveAt(otInstance *aInstance, uint8_t aChannel, uint32_t aStart,
                             uint32_t aDuration)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otError error = OT_ERROR_INVALID_STATE;

    if (radio_inst[pan_idx].sState != OT_RADIO_STATE_DISABLED)
    {
        error  = OT_ERROR_NONE;
        radio_inst[pan_idx].sState = OT_RADIO_STATE_RECEIVE;
#ifdef EXTRA_PROTECTION_EN
        pm_check_inactive("otPlatRadioReceiveAt", true);
#endif
        setChannel(NULL, aChannel, pan_idx);
        // TODO: radio enter RX ready
        if (MAC_STS_SUCCESS == mpan_RxAt(aChannel, aStart, aDuration, pan_idx))
        {
            radio_inst[pan_idx].sStayAwake_b.waitScheduledRxWindow = 1;
            uint64_t now = otPlatTimeGet();
            uint64_t begin = mac_ConvertT0AndDtTo64BitTime(aStart, 0, &now);
            uint64_t end = begin + aDuration;
            /*radio_inst[pan_idx].sWaitScheduledRxWindowTimestamp = end + 1000;*/
            int32_t wakeup2begin = (uint32_t)radio_inst[pan_idx].dbg_exit_dlps - (uint32_t)begin;
            int32_t rx2begin = (uint32_t)now - (uint32_t)begin;

            //otLogNotePlat("wakeup2begin %ld rx2begin %ld begin %llu duration %lu", wakeup2begin, rx2begin, begin, aDuration);
        }
        else
        {
            otLogNotePlat("mpan_RxAt timeout");
        }

        BEE_SleepProcess(aInstance, pan_idx);
    }
    return error;
}
#endif

void BEE_tx_started(uint8_t *p_data, uint8_t pan_idx, uint32_t phrTxTime);
void txProcessSecurity(uint8_t *aFrame, uint8_t pan_idx);
extern uint8_t otMacFrameGetHeaderLength(otRadioFrame *aFrame);
extern uint16_t otMacFrameGetPayloadLength(otRadioFrame *aFrame);
extern uint8_t otMacFrameGetSecurityLevel(otRadioFrame *aFrame);
#define kSwitchedToIndirectTx (1UL << 31)
#define IsSwitchedToIndirectTx(delay) (delay & kSwitchedToIndirectTx)
#define SwitchedToIndirectTxSet(delay) (delay |= kSwitchedToIndirectTx)
#define SwitchedToIndirectTxGet(delay) (delay & ~kSwitchedToIndirectTx)

uint64_t get_tx_backoff_pending(uint8_t pan_idx)
{
    return radio_inst[pan_idx].tx_backoff_pending;
}

void BEE_RadioBackoffStart(uint8_t pan_idx)
{
    uint32_t now_btus;
    uint64_t now;
    uint32_t s = os_lock();
    now = otPlatTimeGetWithBTUS(&now_btus);
    radio_inst[pan_idx].tx_backoff_pending = now + radio_inst[pan_idx].tx_backoff_delay;

#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_US_ALARM)
    if (micro_alarm_us[pan_idx] == 0 ||
        radio_inst[pan_idx].tx_backoff_pending < micro_alarm_us[pan_idx])
    {
        mac_SetBTClkUSInt((pan_idx == 0) ? MAC_BT_TIMER1 : MAC_BT_TIMER5,
                          now_btus + radio_inst[pan_idx].tx_backoff_delay);
        BEE_SleepProcess(NULL, pan_idx);
    }
#elif (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_MS_ALARM)
    if (milli_alarm_us[pan_idx] == 0 ||
        radio_inst[pan_idx].tx_backoff_pending < milli_alarm_us[pan_idx])
    {
        mac_SetBTClkUSInt((pan_idx == 0) ? MAC_BT_TIMER0 : MAC_BT_TIMER4,
                          now_btus + radio_inst[pan_idx].tx_backoff_delay);
        BEE_SleepProcess(NULL, pan_idx);
    }
#endif
    os_unlock(s);
}

void BEE_RadioBackoffTimeout(otInstance *aInstance, uint8_t pan_idx)
{
    uint64_t now, phrTxTime = 0;
    uint32_t s, now_btus;
    otRadioFrame *aFrame = &radio_inst[pan_idx].sTransmitFrame;

    if (!radio_inst[pan_idx].tx_backoff_tmo)
    {
        return;
    }
    radio_inst[pan_idx].tx_backoff_tmo = false;

    now = otPlatTimeGetWithBTUS(&now_btus);
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
    if (IsSwitchedToIndirectTx(aFrame->mInfo.mTxInfo.mTxDelay))
    {
        // Only called for CSL children (sCslPeriod > 0)
        // Note: Our SSEDs "schedule" transmissions to their parent in order to know
        // exactly when in the future the data packets go out so they can calculate
        // the accurate CSL phase to send to their parent.
#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
        if (radio_inst[pan_idx].sCstPeriod > 0)
        {
            phrTxTime = mac_ConvertT0AndDtTo64BitTime(aFrame->mInfo.mTxInfo.mTxDelayBaseTime,
                                                      SwitchedToIndirectTxGet(aFrame->mInfo.mTxInfo.mTxDelay),
                                                      &now);
            if (phrTxTime > now && (phrTxTime - now) > 4000)
            {
                radio_inst[pan_idx].tx_result = TX_AT_FAIL;
                otLogNotePlat("TXAT %u %u too early %lu", aFrame->mPsdu[2], aFrame->mChannel,
                              (uint32_t)(phrTxTime - now));
                BEE_RadioTx(aInstance, pan_idx);
                return;
            }
        }
        else
#endif
        {
            aFrame->mInfo.mTxInfo.mTxDelayBaseTime = (uint32_t)now;
            aFrame->mInfo.mTxInfo.mTxDelay = 2000;
        }
        SwitchedToIndirectTxSet(aFrame->mInfo.mTxInfo.mTxDelay);
    }
#endif

    setChannel(aFrame, aFrame->mChannel, pan_idx);
    mpan_mac_lock(pan_idx);

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    if (otMacFrameIsSecurityEnabled(aFrame) && otMacFrameIsKeyIdMode1(aFrame) &&
        !aFrame->mInfo.mTxInfo.mIsSecurityProcessed)
    {
        otMacFrameSetKeyId(aFrame, radio_inst[pan_idx].sKeyId);
        otMacFrameSetFrameCounter(aFrame, radio_inst[pan_idx].sMacFrameCounter++);
        aFrame->mInfo.mTxInfo.mIsSecurityProcessed = true;
    }

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    if (aFrame->mInfo.mTxInfo.mTxDelay)
        phrTxTime = mac_ConvertT0AndDtTo64BitTime(aFrame->mInfo.mTxInfo.mTxDelayBaseTime,
                                                  SwitchedToIndirectTxGet(aFrame->mInfo.mTxInfo.mTxDelay),
                                                  &now);
    else
    {
        phrTxTime = otPlatAlarmMicroGetNow() + SHR_DURATION_US + 760;
    }
#endif

    BEE_tx_started(NULL, pan_idx, (uint32_t)phrTxTime);
#endif

    if (otMacFrameIsSecurityEnabled(aFrame) && otMacFrameIsKeyIdMode1(aFrame))
    {
        mac_LoadTxNPayload(otMacFrameGetHeaderLength(aFrame),
                           otMacFrameGetHeaderLength(aFrame) + otMacFrameGetPayloadLength(aFrame), &aFrame->mPsdu[0]);
        txProcessSecurity(NULL, pan_idx);
        mac_TrigUpperEnc();
    }
    else
    {
        mac_LoadTxNPayload(0, aFrame->mLength - FCS_LEN, &aFrame->mPsdu[0]);
    }

    if (aFrame->mInfo.mTxInfo.mTxDelay > 0)
    {
        phrTxTime -= SHR_DURATION_US;
        s = os_lock();
        now_btus = mac_GetCurrentBTUS();
        now = bt_clk_offset + now_btus;

        if (phrTxTime > now)
        {
            uint32_t diff = phrTxTime - now;
            mpan_TrigTxNAtUS(otMacFrameIsAckRequested(aFrame), false, true, now_btus + diff, pan_idx);
            os_unlock(s);
            otLogNotePlat("TXAT %u %u remain %lu phrTxTime %llu", aFrame->mPsdu[2], aFrame->mChannel,
                          diff, phrTxTime);
        }
        else
        {
            os_unlock(s);
            mpan_mac_unlock();
            radio_inst[pan_idx].tx_result = TX_AT_FAIL;
            otLogNotePlat("TXAT %u %u expired %lu", aFrame->mPsdu[2], aFrame->mChannel,
                          (uint32_t)(now - phrTxTime));
            BEE_RadioTx(aInstance, pan_idx);
            return;
        }
    }
    else
    {
        mpan_TrigTxN(otMacFrameIsAckRequested(aFrame), false, pan_idx);
        otLogInfoPlat("TX %u %u", aFrame->mPsdu[2], aFrame->mChannel);
    }

    radio_inst[pan_idx].sStayAwake_b.waitTxDone = 1;
    BEE_SleepProcess(NULL, pan_idx);
    os_sem_take(radio_inst[pan_idx].radio_done, 0xffffffff);
    mpan_mac_unlock();
    radio_inst[pan_idx].sStayAwake_b.waitTxDone = 0;
    BEE_SleepProcess(NULL, pan_idx);
    BEE_RadioTx(aInstance, pan_idx);
    return;
}

otError otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();

    if (radio_inst[pan_idx].sState == OT_RADIO_STATE_DISABLED)
    {
        return OT_ERROR_INVALID_STATE;
    }

#ifdef EXTRA_PROTECTION_EN
    pm_check_inactive("otPlatRadioTransmit", true);
#endif

    BEE_EventSend(TX_START, pan_idx);
    otTaskletsSignalPending(aInstance);
    return OT_ERROR_NONE;
}

#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
otError otPlatRadioEnableCst(otInstance         *aInstance,
                             uint32_t            aCstPeriod)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();

    if (radio_inst[pan_idx].sCstPeriod != aCstPeriod)
    {
        radio_inst[pan_idx].sCstPeriod = aCstPeriod;
        otLogNotePlat("%s aCstPeriod %lu", __func__, aCstPeriod);
    }
    return OT_ERROR_NONE;
}

void otPlatRadioUpdateCstSampleTime(otInstance *aInstance, uint64_t aCstSampleTime)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    radio_inst[pan_idx].sCstSampleTime = aCstSampleTime;
    //otLogNotePlat("%s aCstSampleTime %llu", __func__, aCstSampleTime);
}

uint32_t GetNextCstTransmission(uint8_t pan_idx, uint32_t *aTxDelayBaseTime, uint32_t *aTxDelay,
                                uint32_t aAheadUs)
{
    uint64_t radioNow   = otPlatTimeGet();
    uint32_t periodInUs = radio_inst[pan_idx].sCstPeriod;

    uint64_t nextTxWindow  = radio_inst[pan_idx].sCstSampleTime;

    while (nextTxWindow < radioNow + aAheadUs)
    {
        nextTxWindow += periodInUs;
    }

    if (aTxDelayBaseTime)
    {
        *aTxDelayBaseTime = (uint32_t)radioNow;
    }

    if (aTxDelay)
    {
        *aTxDelay = (uint32_t)(nextTxWindow - radioNow);
    }

    radio_inst[pan_idx].sCstSampleTime = nextTxWindow;

    return nextTxWindow - radioNow - aAheadUs;
}
#endif

void HandleTxBackoffDelay(otInstance *aInstance, uint8_t pan_idx)
{
    otRadioFrame *aFrame = &radio_inst[pan_idx].sTransmitFrame;
    radio_inst[pan_idx].tx_backoff_delay = 0;

#if OPENTHREAD_CONFIG_WAKEUP_END_DEVICE_ENABLE
    if (radio_inst[pan_idx].sCstPeriod > 0) // must follow cst scheduler
    {
        radio_inst[pan_idx].tx_backoff_delay = GetNextCstTransmission(pan_idx,
                                                                      &(aFrame->mInfo.mTxInfo.mTxDelayBaseTime), &(aFrame->mInfo.mTxInfo.mTxDelay), 2000);
        SwitchedToIndirectTxSet(aFrame->mInfo.mTxInfo.mTxDelay);

        if (radio_inst[pan_idx].tx_backoff_delay < 1000)
        {
            radio_inst[pan_idx].tx_backoff_delay = 0;
        }
    }
    else
#endif
        if (aFrame->mInfo.mTxInfo.mTxDelay == 0 && radio_inst[pan_idx].sStayAwake_b.rxOnWhenIdle)
        {
            radio_inst[pan_idx].tx_backoff_delay = mac_SetTxNCsmaDetail(true,
                                                                        3 + radio_inst[pan_idx].sTransmitCcaFailCnt);
        }

    // Transmit ASAP or do sw csma backoff
    if (radio_inst[pan_idx].tx_backoff_delay)
    {
        BEE_RadioBackoffStart(pan_idx);
        //otLogNotePlat("TX backoff %lu", radio_inst[pan_idx].tx_backoff_delay);
    }
    else
    {
        radio_inst[pan_idx].tx_backoff_tmo = true;
        otTaskletsSignalPending(aInstance);
    }
}

void BEE_RadioTxStart(otInstance *aInstance, uint8_t pan_idx)
{
    otRadioFrame *aFrame = &radio_inst[pan_idx].sTransmitFrame;
    radio_inst[pan_idx].sState = OT_RADIO_STATE_TRANSMIT;
    radio_inst[pan_idx].sTransmitRetry = 0;
    radio_inst[pan_idx].sTransmitCcaFailCnt = 0;

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    if (radio_inst[pan_idx].sCslPeriod > 0 && aFrame->mInfo.mTxInfo.mTxDelay == 0)
    {
        //otLogNotePlat("SwitchedToIndirectTx");
        SwitchedToIndirectTxSet(aFrame->mInfo.mTxInfo.mTxDelay);
    }
#endif

    HandleTxBackoffDelay(aInstance, pan_idx);
    otPlatRadioTxStarted(aInstance, aFrame);
}

otRadioFrame *otPlatRadioGetTransmitBuffer(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    return &radio_inst[pan_idx].sTransmitFrame;
}

int8_t otPlatRadioGetRssi(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    uint8_t rssi_raw;
    int8_t rssi;

    mpan_mac_lock(pan_idx);
    mac_EDScan_begin(1);
    os_sem_take(radio_inst[pan_idx].radio_done, 0xffffffff);
    mac_EDScan_end(&rssi_raw);
    rssi = mac_edscan_level2dbm(rssi_raw);
    mpan_mac_unlock();

    return rssi;
}

otRadioCaps otPlatRadioGetCaps(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otRadioCaps caps = (OT_RADIO_CAPS_CSMA_BACKOFF | OT_RADIO_CAPS_ACK_TIMEOUT |
                        OT_RADIO_CAPS_SLEEP_TO_TX |
#if !defined(RT_PLATFORM_RTL8922D)
                        OT_RADIO_CAPS_RX_ON_WHEN_IDLE |
#endif
#if TRANSMIT_RETRIES_EN
                        OT_RADIO_CAPS_TRANSMIT_RETRIES |
#endif
#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
                        OT_RADIO_CAPS_TRANSMIT_SEC | OT_RADIO_CAPS_TRANSMIT_TIMING | OT_RADIO_CAPS_RECEIVE_TIMING |
#endif
                        OT_RADIO_CAPS_ENERGY_SCAN);

    return caps;
}

bool otPlatRadioGetPromiscuous(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    if (mac_GetPromiscuous()) { return true; }
    else { return false; }
}

void otPlatRadioSetPromiscuous(otInstance *aInstance, bool aEnable)
{
    OT_UNUSED_VARIABLE(aInstance);
    mac_SetPromiscuous(aEnable);
}

void otPlatRadioEnableSrcMatch(otInstance *aInstance, bool aEnable)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otLogInfoPlat("EnableSrcMatch=%d", aEnable ? 1 : 0);
    if (aEnable) { mpan_SetAddrMatchMode(AUTO_ACK_PENDING_MODE_THREAD, pan_idx); }
    else { mpan_SetAddrMatchMode(AUTO_ACK_PENDING_MODE_ZIGBEE, pan_idx); }
}

otError otPlatRadioAddSrcMatchShortEntry(otInstance *aInstance, uint16_t aShortAddress)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    if (MAC_STS_HW_LIMIT == mpan_AddSrcShortAddrMatch(aShortAddress, radio_inst[pan_idx].sPanid,
                                                      pan_idx))
    {
        otLogInfoPlat("ShortEntry full!");
        return OT_ERROR_NO_BUFS;
    }
    else
    {
        otLogInfoPlat("add %x", aShortAddress);
        return OT_ERROR_NONE;
    }
}

otError otPlatRadioAddSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    if (MAC_STS_HW_LIMIT == mpan_AddSrcExtAddrMatch((uint8_t *)aExtAddress->m8, pan_idx))
    {
        otLogInfoPlat("ExtEntry full!");
        return OT_ERROR_NO_BUFS;
    }
    else
    {
        otLogInfoPlat("add %02x%02x%02x%02x%02x%02x%02x%02x",
                      aExtAddress->m8[7], aExtAddress->m8[6], aExtAddress->m8[5], aExtAddress->m8[4], aExtAddress->m8[3],
                      aExtAddress->m8[2], aExtAddress->m8[1], aExtAddress->m8[0]);
        return OT_ERROR_NONE;
    }
}

otError otPlatRadioClearSrcMatchShortEntry(otInstance *aInstance, uint16_t aShortAddress)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    if (MAC_STS_FAILURE == mpan_DelSrcShortAddrMatch(aShortAddress, radio_inst[pan_idx].sPanid,
                                                     pan_idx))
    {
        otLogInfoPlat("%x not found!", aShortAddress);
        return OT_ERROR_NO_ADDRESS;
    }
    else
    {
        otLogInfoPlat("clear %x", aShortAddress);
        return OT_ERROR_NONE;
    }
}

otError otPlatRadioClearSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    if (MAC_STS_FAILURE == mpan_DelSrcExtAddrMatch((uint8_t *)aExtAddress->m8, pan_idx))
    {
        otLogInfoPlat("%02x%02x%02x%02x%02x%02x%02x%02x not found!",
                      aExtAddress->m8[7], aExtAddress->m8[6], aExtAddress->m8[5], aExtAddress->m8[4],
                      aExtAddress->m8[3], aExtAddress->m8[2], aExtAddress->m8[1], aExtAddress->m8[0]);
        return OT_ERROR_NO_ADDRESS;
    }
    else
    {
        otLogInfoPlat("clear %02x%02x%02x%02x%02x%02x%02x%02x",
                      aExtAddress->m8[7], aExtAddress->m8[6], aExtAddress->m8[5], aExtAddress->m8[4],
                      aExtAddress->m8[3], aExtAddress->m8[2], aExtAddress->m8[1], aExtAddress->m8[0]);
        return OT_ERROR_NONE;
    }
}

void otPlatRadioClearSrcMatchShortEntries(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    mpan_DelAllSrcAddrMatch(pan_idx);
}

void otPlatRadioClearSrcMatchExtEntries(otInstance *aInstance)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    mpan_DelAllSrcAddrMatch(pan_idx);
}

otError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();

    radio_inst[pan_idx].sEnergyDetectionTime = (uint32_t)aScanDuration * 1000UL;
    radio_inst[pan_idx].sEnergyDetectionChannel = aScanChannel;

    BEE_EventSend(ED_SCAN, pan_idx);
    otTaskletsSignalPending(aInstance);
    return OT_ERROR_NONE;
}

otError otPlatRadioGetTransmitPower(otInstance *aInstance, int8_t *aPower)
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error = OT_ERROR_NONE;

    if (aPower == NULL)
    {
        error = OT_ERROR_INVALID_ARGS;
    }
    else
    {
        *aPower = mac_GetTXPower_patch() / 2;
    }

    return error;
}

otError otPlatRadioSetTransmitPower(otInstance *aInstance, int8_t aPower)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aPower);
    radio_inst[pan_idx].sPower = mac_SetTXPower_patch(aPower * 2);
    return OT_ERROR_NONE;
}

otError otPlatRadioGetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t *aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);

    otError              error = OT_ERROR_NONE;
    uint8_t              level;

    if (aThreshold == NULL)
    {
        error = OT_ERROR_INVALID_ARGS;
    }
    else
    {
        level = mac_GetCcaEDThreshold_patch();
        *aThreshold = mac_edscan_level2dbm(level);
    }

    return error;
}

otError otPlatRadioSetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);

    uint8_t level = mac_edscan_dbm2level(aThreshold);
    mac_SetCcaEDThreshold(level);

    return OT_ERROR_NONE;
}

otError otPlatRadioGetFemLnaGain(otInstance *aInstance, int8_t *aGain)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    otError error = OT_ERROR_NONE;

    if (aGain == NULL)
    {
        error = OT_ERROR_INVALID_ARGS;
    }
    else
    {
        *aGain = radio_inst[pan_idx].sLnaGain;
    }

    return error;
}

otError otPlatRadioSetFemLnaGain(otInstance *aInstance, int8_t aGain)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    int8_t  threshold;
    int8_t  oldLnaGain = radio_inst[pan_idx].sLnaGain;
    otError error      = OT_ERROR_NONE;

    error = otPlatRadioGetCcaEnergyDetectThreshold(aInstance, &threshold);
    otEXPECT(error == OT_ERROR_NONE);

    radio_inst[pan_idx].sLnaGain = aGain;
    error    = otPlatRadioSetCcaEnergyDetectThreshold(aInstance, threshold);
    otEXPECT_ACTION(error == OT_ERROR_NONE, radio_inst[pan_idx].sLnaGain = oldLnaGain);

exit:
    return error;
}

void BEE_WakeupProcess(otInstance *aInstance, uint8_t pan_idx)
{
#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
#if defined(BUILD_MATTER) || defined(BUILD_BLE_PERIPHERAL)
    extern bool matter_ble_check_connected(void);
    if (matter_ble_check_connected())
    {
        otLogNotePlat("matter_ble_check_connected");
    }
    else
    {
        if (app_wakeup_reason != APP_WAKEUP_REASON_NONE)
        {
            app_wakeup_reason = APP_WAKEUP_REASON_NONE;
        }
        else
        {
            BEE_SleepDirect();
        }
    }
#endif
#endif
}

#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
static void zbpm_enter_pan0(void)
{
}

static void zbpm_enter_pan1(void)
{
}

static void zbpm_exit_pan0(void)
{
    zbpm_adapter_t *padapter = &zbpm_adap;
    uint32_t curr_us;
    mac_bt_clk_t bt_clk_overflow_value =
    {
        .bt_clk_counter = 0,        /*!< [9..0] BT clock counter in 1 us unit */
        .bt_native_clk = 0x3FFFFF   /*!< [31..10] BT native clock[21:1] in unit of BT slot */
    };

    curr_us = mac_GetCurrentBTUS();
    if (curr_us < padapter->enter_time_us)
    {
        bt_clk_offset += MAX_BT_CLOCK_COUNTER;
    }
    mac_SetBTClkInt(MAC_BT_TIMER3, &bt_clk_overflow_value);
    mac_notify_pm_wakeup(0);
}

static void zbpm_exit_pan1(void)
{
    zbpm_adapter_t *padapter = &zbpm_adap;
    uint32_t curr_us;
    mac_bt_clk_t bt_clk_overflow_value =
    {
        .bt_clk_counter = 0,        /*!< [9..0] BT clock counter in 1 us unit */
        .bt_native_clk = 0x3FFFFF   /*!< [31..10] BT native clock[21:1] in unit of BT slot */
    };

    curr_us = mac_GetCurrentBTUS();
    if (curr_us < padapter->enter_time_us)
    {
        bt_clk_offset += MAX_BT_CLOCK_COUNTER;
    }
    mac_SetBTClkInt(MAC_BT_TIMER3, &bt_clk_overflow_value);
    mac_notify_pm_wakeup(1);
}

void mac_report_pm_wakeup(uint8_t pan_idx)
{
    if (radio_inst[pan_idx].sCslPeriod > 0)
    {
        radio_inst[pan_idx].dbg_exit_dlps = otPlatTimeGet();
        mac_RadioOn();
    }

    handle_tx_backoff_pending(pan_idx);
    micro_handler(pan_idx);
    milli_handler(pan_idx);
}
#endif

bool mac_report_pm_check_result(uint8_t pan_idx)
{
    if (radio_inst[pan_idx].sStayAwake)
    {
        if (radio_inst[pan_idx].sStayAwake & 0xf)
        {
            otLogDebgPlat("sStayAwake 0x%x", radio_inst[pan_idx].sStayAwake);
        }
        return false;
    }
    else
    {
        return true;
    }
}

void BEE_SleepProcess(otInstance *aInstance, uint8_t pan_idx)
{
#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
    volatile zbpm_adapter_t *padapter = &zbpm_adap;
    uint64_t now;
    uint32_t now_btus;
    uint32_t s;
    uint32_t diff;

    if (false == zbpm_inited)
    {
        return;
    }

#ifdef EXTRA_PROTECTION_EN
    pm_check_inactive("BEE_SleepProcess", true);
#endif

    s = os_lock();
    now = otPlatTimeGetWithBTUS(&now_btus);

    // Get the next valid (non-zero) alarm time among milli, micro, and tx_backoff
    uint64_t tmp_ms = (milli_alarm_us[pan_idx] > now) ? milli_alarm_us[pan_idx] : 0;
    uint64_t tmp_us = (micro_alarm_us[pan_idx] > now) ? micro_alarm_us[pan_idx] : 0;
    uint64_t tmp_tx_backoff = (radio_inst[pan_idx].tx_backoff_pending > now) ?
                              radio_inst[pan_idx].tx_backoff_pending : 0;

    // Merge tx_backoff with the appropriate timer (choose earliest non-zero time)
#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_US_ALARM)
    if (tmp_tx_backoff != 0 && (tmp_us == 0 || tmp_tx_backoff < tmp_us))
    {
        tmp_us = tmp_tx_backoff;
    }
#elif (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_MS_ALARM)
    if (tmp_tx_backoff != 0 && (tmp_ms == 0 || tmp_tx_backoff < tmp_ms))
    {
        tmp_ms = tmp_tx_backoff;
    }
#endif

    if (!tmp_ms && !tmp_us)
    {
        os_unlock(s);
        goto stay_awake;
    }

    // Default: use micro alarm if available
    diff = tmp_us ? tmp_us - now : UINT32_MAX;

    // If milli alarm is valid and triggers at least 1 ms earlier than micro alarm, use milli alarm instead
    if (tmp_ms && (!tmp_us || tmp_us > tmp_ms + 1000))
    {
        diff = tmp_ms - now;
    }

    if (diff < 10000)
    {
        os_unlock(s);
        goto stay_awake;
    }

    if (diff > ((MAX_BT_CLOCK_COUNTER >> 1) - 1))
    {
        diff = (MAX_BT_CLOCK_COUNTER >> 1) - 1;
    }

    padapter->cfg.wake_interval_en = 0;
    padapter->cfg.stage_time_learned = 0;
    padapter->wakeup_time_us = now_btus + diff;
    padapter->wakeup_interval_us = 0;

    padapter->enter_callback = (pan_idx == 0) ? zbpm_enter_pan0 : zbpm_enter_pan1;
    padapter->exit_callback = (pan_idx == 0) ? zbpm_exit_pan0 : zbpm_exit_pan1;

    padapter->wakeup_reason = ZBMAC_PM_WAKEUP_UNKNOWN;
    padapter->error_code = ZBMAC_PM_ERROR_UNKNOWN;
    padapter->power_mode = ZBMAC_DEEP_SLEEP;
    //otLogNotePlat("sleep %u us tmp_us %llu", diff, tmp_us);

    os_unlock(s);
    return;

stay_awake:
    s = os_lock();
    if (padapter->power_mode == ZBMAC_DEEP_SLEEP)
    {
        padapter->power_mode = ZBMAC_ACTIVE;
    }
    os_unlock(s);
#endif
}

#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
static void direct_exit_pan0(void)
{
    zbpm_adapter_t *padapter = &zbpm_adap;
    uint32_t curr_us = mac_GetCurrentBTUS();
    mac_bt_clk_t bt_clk_overflow_value =
    {
        .bt_clk_counter = 0,        /*!< [9..0] BT clock counter in 1 us unit */
        .bt_native_clk = 0x3FFFFF   /*!< [31..10] BT native clock[21:1] in unit of BT slot */
    };

    if (curr_us < padapter->enter_time_us)
    {
        bt_clk_offset += MAX_BT_CLOCK_COUNTER;
    }
    mac_SetBTClkInt(MAC_BT_TIMER3, &bt_clk_overflow_value);
    if (app_wakeup_reason == APP_WAKEUP_REASON_UART_RX)
    {
        BEE_EventSend(UART_RX, 0);
    }
    else
    {
        BEE_EventSend(WAKEUP, 0);
    }
    otTaskletsSignalPending(NULL);
}

static void direct_exit_pan1(void)
{
    zbpm_adapter_t *padapter = &zbpm_adap;
    uint32_t curr_us = mac_GetCurrentBTUS();
    mac_bt_clk_t bt_clk_overflow_value =
    {
        .bt_clk_counter = 0,        /*!< [9..0] BT clock counter in 1 us unit */
        .bt_native_clk = 0x3FFFFF   /*!< [31..10] BT native clock[21:1] in unit of BT slot */
    };

    if (curr_us < padapter->enter_time_us)
    {
        bt_clk_offset += MAX_BT_CLOCK_COUNTER;
    }
    mac_SetBTClkInt(MAC_BT_TIMER3, &bt_clk_overflow_value);
    if (app_wakeup_reason == APP_WAKEUP_REASON_UART_RX)
    {
        app_wakeup_reason = APP_WAKEUP_REASON_NONE;
        BEE_EventSend(UART_RX, 1);
    }
    else
    {
        BEE_EventSend(WAKEUP, 1);
    }
    otTaskletsSignalPending(NULL);
}
#endif

void BEE_SleepDirect(void)
{
#if !defined(_IS_FPGA_) && defined(DLPS_EN) && (DLPS_EN == 1)
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    zbpm_adapter_t *padapter = &zbpm_adap;
    uint32_t curr_us;
    uint32_t s;

    curr_us = mac_GetCurrentBTUS();
    s = os_lock();
    padapter->power_mode = ZBMAC_DEEP_SLEEP;
    padapter->wakeup_reason = ZBMAC_PM_WAKEUP_UNKNOWN;
    padapter->error_code = ZBMAC_PM_ERROR_UNKNOWN;

    padapter->stage_time[ZBMAC_PM_CHECK] = 20;
    padapter->stage_time[ZBMAC_PM_STORE] = 15;
    padapter->stage_time[ZBMAC_PM_ENTER] = 5;
    padapter->stage_time[ZBMAC_PM_EXIT] = 5;
    padapter->stage_time[ZBMAC_PM_RESTORE] = 20;
    padapter->minimum_sleep_time = 20;
    padapter->learning_guard_time = 7; // 3 (learning guard time) + 4 (two 16k po_intr drift)

    padapter->cfg.wake_interval_en = 0;
    padapter->cfg.stage_time_learned = 0;
    padapter->wakeup_time_us = curr_us + (MAX_BT_CLOCK_COUNTER >> 1) - 1;
    padapter->wakeup_interval_us = 0;

    //padapter->enter_callback = zbpm_enter_direct;
    padapter->exit_callback = (pan_idx == 0) ? direct_exit_pan0 : direct_exit_pan1;
    if (!zbpm_inited)
    {
        zbpm_inited = true;
        zbmac_pm_init(padapter);
    }
    os_unlock(s);
    otLogNotePlat("direct sleep to %u", curr_us + (MAX_BT_CLOCK_COUNTER >> 1) - 1);
#endif
}

void mpan_RxAt_done_callback(uint8_t pan_idx)
{
#if defined(DLPS_EN) && (DLPS_EN == 1)
    radio_inst[pan_idx].sStayAwake_b.waitScheduledRxWindow = 0;
#endif
}

void BEE_AlarmMicroProcess(otInstance *aInstance, uint8_t pan_idx)
{
#if 0 // debug purpose
    uint64_t now = otPlatTimeGet();
    otLogNotePlat("us fired %lld", now - micro_alarm_us[pan_idx]);
#endif
    micro_alarm_us[pan_idx] = 0;
    otPlatAlarmMicroFired(aInstance);
}

void BEE_AlarmMilliProcess(otInstance *aInstance, uint8_t pan_idx)
{
#if 0 // debug purpose
    uint64_t now = otPlatTimeGet();
    otLogNotePlat("ms fired %lld", now - milli_alarm_us[pan_idx]);
#endif
    milli_alarm_us[pan_idx] = 0;
    otPlatAlarmMilliFired(aInstance);
}

bool readFrame(rx_item_t *item, uint8_t pan_idx);
void BEE_RadioRx(otInstance *aInstance, uint8_t pan_idx)
{
    rx_item_t *rx_item;

    while (radio_inst[pan_idx].rx_head != radio_inst[pan_idx].rx_tail)
    {
        rx_item = &radio_inst[pan_idx].rx_queue[radio_inst[pan_idx].rx_head];
        readFrame(rx_item, pan_idx);
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
        if (radio_inst[pan_idx].sCslPeriod > 0)
        {
            otRadioFrame *receivedFrame = &rx_item->sReceivedFrames;
            uint32_t period = radio_inst[pan_idx].sCslPeriod * OT_US_PER_TEN_SYMBOLS;
            uint32_t sampleTime = radio_inst[pan_idx].sCslSampleTime - period;
            int32_t deviation = (uint32_t)receivedFrame->mInfo.mRxInfo.mTimestamp - sampleTime;
            //otLogNotePlat("readFrame %u %u dev %ld period %lu", receivedFrame->mPsdu[2], receivedFrame->mChannel, deviation, period);
        }
#endif
        //p_fc = (fc_t *)&rx_item->sReceivedPsdu[1];
        otPlatRadioReceiveDone(aInstance, &rx_item->sReceivedFrames, OT_ERROR_NONE);
        radio_inst[pan_idx].rx_head = (radio_inst[pan_idx].rx_head + 1) % RX_BUF_SIZE;
    }
}

#if defined(DLPS_EN) && (DLPS_EN == 1)
void waitRxPendingDataTimeout(void *arg)
{
    uint8_t pan_idx = (uint32_t)arg;

    if (radio_inst[pan_idx].sStayAwake_b.waitRxPendingData)
    {
        radio_inst[pan_idx].sStayAwake_b.waitRxPendingData = 0;
    }
}
#endif

void BEE_RadioTx(otInstance *aInstance, uint8_t pan_idx)
{
    fc_t *p_fc = NULL;
    otRadioFrame *aAckFrame = NULL;
    otError aError = OT_ERROR_NONE;

    do
    {
        switch (radio_inst[pan_idx].tx_result)
        {
        case TX_OK:
            {
                otLogInfoPlat("TX_OK %u %u cca_fail %lu",
                              radio_inst[pan_idx].sTransmitFrame.mPsdu[2],
                              radio_inst[pan_idx].sTransmitFrame.mChannel,
                              radio_inst[pan_idx].sTransmitCcaFailCnt);
                goto exit;
            }
            break;

        case TX_WAIT_ACK:
            if (radio_inst[pan_idx].ack_receive_done)
            {
                radio_inst[pan_idx].ack_receive_done = false;
                otLogInfoPlat("TX_WAIT_ACK %u %u cca_fail %lu",
                              radio_inst[pan_idx].sTransmitFrame.mPsdu[2],
                              radio_inst[pan_idx].sTransmitFrame.mChannel,
                              radio_inst[pan_idx].sTransmitCcaFailCnt);
                if (otMacFrameIsVersion2015(&radio_inst[pan_idx].sTransmitFrame))
                {
#if defined(RT_PLATFORM_BEE3PLUS)
                    uint8_t ie_hdr_len;
                    uint8_t *p_frame;
                    uint8_t *p_ie_hdr;
                    readFrame(&radio_inst[pan_idx].ack_received, pan_idx);
                    p_fc = (fc_t *)&radio_inst[pan_idx].ack_received.sReceivedPsdu[1];
                    p_frame = radio_inst[pan_idx].ack_received.sReceivedPsdu;
                    p_ie_hdr = mac_802154_frame_parser_ie_header_get(p_frame);
                    if (p_ie_hdr)
                    {
                        ie_hdr_len = radio_inst[pan_idx].ack_received.sReceivedFrames.mPsdu +
                                     otMacFrameGetHeaderLength(&radio_inst[pan_idx].ack_received.sReceivedFrames) -
                                     p_ie_hdr;
                    }
                    radio_inst[pan_idx].ack_item.sReceivedFrames.mPsdu = &radio_inst[pan_idx].ack_item.sReceivedPsdu[1];
                    otMacFrameGenerateImmAck(&radio_inst[pan_idx].sTransmitFrame, p_fc->pending,
                                             &radio_inst[pan_idx].ack_item.sReceivedFrames);
                    p_fc = (fc_t *)&radio_inst[pan_idx].ack_item.sReceivedPsdu[1];
                    if (p_ie_hdr)
                    {
                        memcpy(&radio_inst[pan_idx].ack_item.sReceivedFrames.mPsdu[3], p_ie_hdr, ie_hdr_len);
                        p_fc->ie_present = 1;
                        radio_inst[pan_idx].ack_item.sReceivedFrames.mLength += ie_hdr_len;
                    }
                    p_fc->ver = FRAME_VER_2015;
                    aAckFrame = &radio_inst[pan_idx].ack_item.sReceivedFrames;
#else
                    readFrame(&radio_inst[pan_idx].ack_item, pan_idx);
                    p_fc = (fc_t *)&radio_inst[pan_idx].ack_item.sReceivedPsdu[1];
                    aAckFrame = &radio_inst[pan_idx].ack_item.sReceivedFrames;
#endif
                }
                else
                {
                    radio_inst[pan_idx].ack_item.sReceivedFrames.mPsdu = &radio_inst[pan_idx].ack_item.sReceivedPsdu[1];
                    otMacFrameGenerateImmAck(&radio_inst[pan_idx].sTransmitFrame, radio_inst[pan_idx].ack_fp,
                                             &radio_inst[pan_idx].ack_item.sReceivedFrames);
                    p_fc = (fc_t *)&radio_inst[pan_idx].ack_item.sReceivedPsdu[1];
                    aAckFrame = &radio_inst[pan_idx].ack_item.sReceivedFrames;
                }
#if defined(DLPS_EN) && (DLPS_EN == 1)
                if (p_fc->pending == 1 && radio_inst[pan_idx].sWaitRxPendingDataTimer)
                {
                    radio_inst[pan_idx].sStayAwake_b.waitRxPendingData = 1;
                    mac_SoftwareTimer_Start(radio_inst[pan_idx].sWaitRxPendingDataTimer,
                                            mac_GetCurrentBTUS() + OPENTHREAD_CONFIG_MAC_DATA_POLL_TIMEOUT * 1000,
                                            waitRxPendingDataTimeout, (void *)(uint32_t)pan_idx);
                }
                //radio_inst[pan_idx].sWaitRxPendingDataTimestamp = otPlatTimeGet() + OPENTHREAD_CONFIG_MAC_DATA_POLL_TIMEOUT * 1000;
#endif
                goto exit;
            }

        case TX_NO_ACK:
            {
                mac_PTA_Wrokaround();
                otLogNotePlat("TX_NO_ACK %u %u cca_fail %lu",
                              radio_inst[pan_idx].sTransmitFrame.mPsdu[2],
                              radio_inst[pan_idx].sTransmitFrame.mChannel,
                              radio_inst[pan_idx].sTransmitCcaFailCnt);
                mac_txnak_priv_hanlder();
                aError = OT_ERROR_NO_ACK;
#if (TRANSMIT_RETRIES_EN == 1)
                if (otMacFrameIsAckRequested(&radio_inst[pan_idx].sTransmitFrame))
                {
                    radio_inst[pan_idx].sTransmitRetry++;
                }

                if (radio_inst[pan_idx].sTransmitRetry == MAX_TRANSMIT_RETRY ||
                    (radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mTxDelay > 0
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
                     && !IsSwitchedToIndirectTx(radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mTxDelay)
#endif
                    ))
                {
                    goto exit;
                }

                radio_inst[pan_idx].sTransmitCcaFailCnt = 0;
                HandleTxBackoffDelay(aInstance, pan_idx);
#else
                goto exit;
#endif
            }
            break;

        case TX_AT_FAIL:
            {
                otLogNotePlat("TX_AT_FAIL %u %u cca_fail %lu",
                              radio_inst[pan_idx].sTransmitFrame.mPsdu[2],
                              radio_inst[pan_idx].sTransmitFrame.mChannel,
                              radio_inst[pan_idx].sTransmitCcaFailCnt);
                if (radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mTxDelay > 0
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
                    && !IsSwitchedToIndirectTx(radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mTxDelay)
#endif
                   )
                {
                    aError = OT_ERROR_NO_ACK;
                    goto exit;
                }
                radio_inst[pan_idx].sTransmitCcaFailCnt++;
            }
        case TX_TERMED:
            {
                if (radio_inst[pan_idx].tx_result == TX_TERMED)
                {
                    otLogNotePlat("TX_TERMED %u %u cca_fail %lu",
                                  radio_inst[pan_idx].sTransmitFrame.mPsdu[2],
                                  radio_inst[pan_idx].sTransmitFrame.mChannel,
                                  radio_inst[pan_idx].sTransmitCcaFailCnt);
                }
            }
        case TX_CCA_FAIL:
            {
                if (radio_inst[pan_idx].tx_result == TX_CCA_FAIL)
                {
                    otLogNotePlat("TX_CCA_FAIL %u %u cca_fail %lu",
                                  radio_inst[pan_idx].sTransmitFrame.mPsdu[2],
                                  radio_inst[pan_idx].sTransmitFrame.mChannel,
                                  radio_inst[pan_idx].sTransmitCcaFailCnt);
                    radio_inst[pan_idx].sTransmitCcaFailCnt++;
                }
            }
        case TX_PTA_FAIL:
            {
                mac_PTA_Wrokaround();
                if (radio_inst[pan_idx].tx_result == TX_PTA_FAIL)
                {
                    otLogNotePlat("TX_PTA_FAIL %u %u cca_fail %lu",
                                  radio_inst[pan_idx].sTransmitFrame.mPsdu[2],
                                  radio_inst[pan_idx].sTransmitFrame.mChannel,
                                  radio_inst[pan_idx].sTransmitCcaFailCnt);
                }

                if (radio_inst[pan_idx].sTransmitCcaFailCnt == MAX_TRANSMIT_CCAFAIL)
                {
                    aError = OT_ERROR_CHANNEL_ACCESS_FAILURE;
                    goto exit;
                }

                HandleTxBackoffDelay(aInstance, pan_idx);
            }
            break;

        default:
            break;
        }
    }
    while (0);
    return;

exit:
    radio_inst[pan_idx].sState = OT_RADIO_STATE_RECEIVE;
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    if (IsSwitchedToIndirectTx(radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mTxDelay))
    {
        radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mTxDelay = 0;
        radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mTxDelayBaseTime = 0;
    }
#endif
    otPlatRadioTxDone(aInstance, &radio_inst[pan_idx].sTransmitFrame, aAckFrame, aError);
    return;
}

void BEE_RadioEnergyScan(otInstance *aInstance, uint8_t pan_idx)
{
    uint8_t ed_value_raw;
    int8_t ed_value;

    do
    {
        setChannel(NULL, radio_inst[pan_idx].sEnergyDetectionChannel, pan_idx);
        mpan_mac_lock(pan_idx);
        mac_EDScan_begin(radio_inst[pan_idx].sEnergyDetectionTime / 128);
        os_sem_take(radio_inst[pan_idx].radio_done, 0xffffffff);
        mac_EDScan_end(&ed_value_raw);
        ed_value = mac_edscan_level2dbm(ed_value_raw);
        mpan_mac_unlock();
        otPlatRadioEnergyScanDone(aInstance, ed_value);
    }
    while (0);
}

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
static uint16_t getCslPhase(uint8_t pan_idx, uint32_t phrTxTime)
{
    if (phrTxTime == 0)
    {
        phrTxTime = (uint32_t)otPlatTimeGet();
    }

    uint32_t cslPeriodInUs = radio_inst[pan_idx].sCslPeriod * OT_US_PER_TEN_SYMBOLS;
    uint32_t diff = ((radio_inst[pan_idx].sCslSampleTime % cslPeriodInUs)
                     - (phrTxTime % cslPeriodInUs) + cslPeriodInUs) % cslPeriodInUs;
    return (uint16_t)(diff / OT_US_PER_TEN_SYMBOLS);
}
#endif

APP_RAM_TEXT_SECTION void BEE_tx_ack_started(uint8_t *p_data, int8_t power, uint8_t lqi,
                                             uint8_t pan_idx, uint32_t phrTxTime)
{
    otRadioFrame ackFrame;
    csl_ie_t *p_csl_ie;
    vendor_ie_t *p_vendor_ie;
#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    uint8_t      linkMetricsDataLen = 0;
    uint8_t      linkMetricsData[OT_ENH_PROBING_IE_DATA_MAX_SIZE];
    otMacAddress macAddress;
#else
    OT_UNUSED_VARIABLE(power);
    OT_UNUSED_VARIABLE(lqi);
#endif

    ackFrame.mPsdu   = &p_data[1];
    ackFrame.mLength = p_data[0];

    // Check if the frame pending bit is set in ACK frame.
    //sEnhAckdWithFramePending = p_data[FRAME_PENDING_OFFSET] & FRAME_PENDING_BIT;

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    // Update IE and secure Enh-ACK.
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    if (radio_inst[pan_idx].sCslPeriod > 0)
    {
        p_csl_ie = (csl_ie_t *)&radio_inst[pan_idx].sEnhAckPsdu[MAC_FRAME_TX_HDR_LEN +
                                                                radio_inst[pan_idx].sCslIeIndex];
        p_csl_ie->phase = getCslPhase(pan_idx, phrTxTime);
        p_csl_ie->period = radio_inst[pan_idx].sCslPeriod;
    }
#endif

#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    if (radio_inst[pan_idx].enhAckProbingDataLen > 0)
    {
        otMacFrameGetDstAddr(&ackFrame, &macAddress);
        if ((linkMetricsDataLen = otLinkMetricsEnhAckGenData(&macAddress, lqi, power, linkMetricsData)) > 0)
        {
            p_vendor_ie = (vendor_ie_t *)&radio_inst[pan_idx].sEnhAckPsdu[MAC_FRAME_TX_HDR_LEN +
                                                                          radio_inst[pan_idx].sVendorIeIndex];
            p_vendor_ie->oui[0] = 0x9b;
            p_vendor_ie->oui[1] = 0xb8;
            p_vendor_ie->oui[2] = 0xea;
            p_vendor_ie->subtype = 0;
            mac_memcpy(&p_vendor_ie->content, linkMetricsData, linkMetricsDataLen);
        }
    }
#endif
#endif
}

bool readFrame(rx_item_t *item, uint8_t pan_idx)
{
    otRadioFrame *receivedFrame = &item->sReceivedFrames;
    pmac_rxfifo_t prx_fifo = (pmac_rxfifo_t)&item->sReceivedPsdu[0];
    mac_rxfifo_tail_t *prx_fifo_tail;
    uint8_t channel;
    uint8_t bt_channel;
    int8_t rssi;
    uint8_t lqi;
    uint8_t *p_data;
    otRadioFrame ackedFrame;

#if 0
    if (mac_GetRxFrmSecEn() && mac_GetRxFrmAckReq() && 0x2 == mac_GetRxFrmSecKeyIdMode())
    {
        return false;
    }
#endif

    //mac_Rx((uint8_t *)prx_fifo);
    prx_fifo_tail = (mac_rxfifo_tail_t *)&item->sReceivedPsdu[1 + prx_fifo->frm_len];
    channel = radio_inst[pan_idx].rx_queue[radio_inst[pan_idx].rx_tail].sReceivedChannel;
    // convert 15.4's channel to BT's channel index, which is started from 2402 MHz
    bt_channel = ((channel - 10) * 5) - 2; // 15.4 channel is started from channell (2405 MHz)
    rssi = mac_GetRSSIFromRaw(prx_fifo_tail->rssi, bt_channel);
    lqi = prx_fifo_tail->lqi;

    p_data = &item->sReceivedPsdu[0];
    receivedFrame->mPsdu               = &p_data[1];
    receivedFrame->mLength             = p_data[0];
    receivedFrame->mChannel            = channel;
    receivedFrame->mInfo.mRxInfo.mRssi = rssi;
    receivedFrame->mInfo.mRxInfo.mLqi  = lqi;

    // Inform if this frame was acknowledged with frame pending set.
    //receivedFrame->mInfo.mRxInfo.mAckedWithFramePending = radio_inst[pan_idx].sAckedWithFramePending;

    // 0x7E header cmdid propid STREAM_RAW crc16 0x7E
    // Get the timestamp when the SFD was received
    receivedFrame->mInfo.mRxInfo.mTimestamp = bt_clk_offset + mac_BTClkToUS(prx_fifo_tail->bt_time) -
                                              (PHY_SYMBOL_TIME * 2);
    // Inform if this frame was acknowledged with secured Enh-ACK.
    if (p_data[ACK_REQUEST_OFFSET] & ACK_REQUEST_BIT)
    {
        if (otMacFrameIsVersion2015(receivedFrame))
        {
            ackedFrame.mLength = radio_inst[pan_idx].sEnhAckPsdu[1];
            ackedFrame.mPsdu = &radio_inst[pan_idx].sEnhAckPsdu[2];
            receivedFrame->mInfo.mRxInfo.mAckedWithFramePending = ackedFrame.mPsdu[0] & FRAME_PENDING_BIT;
            receivedFrame->mInfo.mRxInfo.mAckedWithSecEnhAck = otMacFrameIsSecurityEnabled(&ackedFrame);
            receivedFrame->mInfo.mRxInfo.mAckFrameCounter    = otMacFrameGetFrameCounter(&ackedFrame);
            receivedFrame->mInfo.mRxInfo.mAckKeyId           = otMacFrameGetKeyId(&ackedFrame);
        }
        else
        {
            if (mac_GetSrcMatchStatus())
            {
                mac_ClrSrcMatchStatus();
                receivedFrame->mInfo.mRxInfo.mAckedWithFramePending = true;
            }
            else
            {
                receivedFrame->mInfo.mRxInfo.mAckedWithFramePending = false;
            }
        }
    }
    else
    {
        receivedFrame->mInfo.mRxInfo.mAckedWithFramePending = false;
    }

#if defined(MAC_STATS_PKTSIG_EN) && (MAC_STATS_PKTSIG_EN != 0)
    {
        mac_stats_pktsig_update(rssi, lqi);
    }
#endif

    return true;
}

int8_t otPlatRadioGetReceiveSensitivity(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return SBEE2_RECEIVE_SENSITIVITY;
}

#if OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT
void BEE_tx_started(uint8_t *p_data, uint8_t pan_idx, uint32_t phrTxTime)
{
    OT_UNUSED_VARIABLE(p_data);

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    if (radio_inst[pan_idx].sCslPeriod > 0)
    {
        otMacFrameSetCslIe(&radio_inst[pan_idx].sTransmitFrame, (uint16_t)radio_inst[pan_idx].sCslPeriod,
                           getCslPhase(pan_idx, phrTxTime));
    }
#endif

    // Update IE and secure transmit frame
#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
    if (sTransmitFrame.mInfo.mTxInfo.mIeInfo->mTimeIeOffset != 0)
    {
        uint8_t *timeIe = sTransmitFrame.mPsdu + sTransmitFrame.mInfo.mTxInfo.mIeInfo->mTimeIeOffset;
        uint64_t time   = otPlatTimeGet() + sTransmitFrame.mInfo.mTxInfo.mIeInfo->mNetworkTimeOffset;

        *timeIe = sTransmitFrame.mInfo.mTxInfo.mIeInfo->mTimeSyncSeq;

        *(++timeIe) = (uint8_t)(time & 0xff);
        for (uint8_t i = 1; i < sizeof(uint64_t); i++)
        {
            time        = time >> 8;
            *(++timeIe) = (uint8_t)(time & 0xff);
        }
    }
#endif // OPENTHREAD_CONFIG_TIME_SYNC_ENABLE

    return;
}
#endif

void txProcessSecurity(uint8_t *aFrame, uint8_t pan_idx)
{
    OT_UNUSED_VARIABLE(aFrame);
#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    radio_inst[pan_idx].sTransmitFrame.mInfo.mTxInfo.mAesKey = &radio_inst[pan_idx].sCurrKey;

    struct
    {
        uint8_t sec_level;
        uint32_t frame_counter;
        uint64_t src_ext_addr;
    } __attribute__((packed)) nonce;
    nonce.sec_level = otMacFrameGetSecurityLevel(&radio_inst[pan_idx].sTransmitFrame);
    nonce.frame_counter = otMacFrameGetFrameCounter(&radio_inst[pan_idx].sTransmitFrame);
    mac_memcpy((uint8_t *)&nonce.src_ext_addr, mpan_GetLongAddress(pan_idx),
               sizeof(nonce.src_ext_addr));

    // set nonce
    mac_LoadNonce((uint8_t *)&nonce);
    // set key
    mac_LoadTxNKey(radio_inst[pan_idx].sCurrKey.mKeyMaterial.mKey.m8);
    // set security level
    mac_SetTxNCipher(nonce.sec_level);
#endif
}

uint64_t otPlatRadioGetNow(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return otPlatTimeGet();
}

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
void otPlatRadioSetMacKey(otInstance             *aInstance,
                          uint8_t                 aKeyIdMode,
                          uint8_t                 aKeyId,
                          const otMacKeyMaterial *aPrevKey,
                          const otMacKeyMaterial *aCurrKey,
                          const otMacKeyMaterial *aNextKey,
                          otRadioKeyType          aKeyType)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aKeyIdMode);

    assert(aKeyType == OT_KEY_TYPE_LITERAL_KEY);
    assert(aPrevKey != NULL && aCurrKey != NULL && aNextKey != NULL);

    uint32_t s = os_lock();

    radio_inst[pan_idx].sKeyId   = aKeyId;
    radio_inst[pan_idx].sPrevKey = *aPrevKey;
    radio_inst[pan_idx].sCurrKey = *aCurrKey;
    radio_inst[pan_idx].sNextKey = *aNextKey;

    os_unlock(s);
}

void otPlatRadioSetMacFrameCounter(otInstance *aInstance, uint32_t aMacFrameCounter)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    uint32_t s = os_lock();

    radio_inst[pan_idx].sMacFrameCounter = aMacFrameCounter;

    os_unlock(s);
}

void otPlatRadioSetMacFrameCounterIfLarget(otInstance *aInstance, uint32_t aMacFrameCounter)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    uint32_t s = os_lock();

    if (aMacFrameCounter > radio_inst[pan_idx].sMacFrameCounter)
    {
        radio_inst[pan_idx].sMacFrameCounter = aMacFrameCounter;
    }

    os_unlock(s);
}
#endif // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2

#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
static void updateIeData(otInstance *aInstance, otShortAddress aShortAddr,
                         const otExtAddress *aExtAddr, uint8_t pan_idx)
{
    OT_UNUSED_VARIABLE(aInstance);

    otMacAddress macAddress;
    macAddress.mType                  = OT_MAC_ADDRESS_TYPE_SHORT;
    macAddress.mAddress.mShortAddress = aShortAddr;

    radio_inst[pan_idx].enhAckProbingDataLen = otLinkMetricsEnhAckGetDataLen(&macAddress);
}
#endif // OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
otError otPlatRadioEnableCsl(otInstance         *aInstance,
                             uint32_t            aCslPeriod,
                             otShortAddress      aShortAddr,
                             const otExtAddress *aExtAddr)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    radio_inst[pan_idx].sCslPeriod = aCslPeriod;
    otLogNotePlat("%s aCslPeriod %lu", __func__, aCslPeriod);
    return OT_ERROR_NONE;
}

void otPlatRadioUpdateCslSampleTime(otInstance *aInstance, uint32_t aCslSampleTime)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    radio_inst[pan_idx].sCslSampleTime = aCslSampleTime;
}

uint8_t otPlatRadioGetCslAccuracy(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return otPlatTimeGetXtalAccuracy();
}

uint8_t otPlatRadioGetCslUncertainty(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return CSL_UNCERT;
}

uint8_t otPlatRadioGetCslClockUncertainty(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return CSL_UNCERT;
}
#endif // OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE

#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
otError otPlatRadioConfigureEnhAckProbing(otInstance          *aInstance,
                                          otLinkMetrics        aLinkMetrics,
                                          const otShortAddress aShortAddress,
                                          const otExtAddress *aExtAddress)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aLinkMetrics);
    OT_UNUSED_VARIABLE(aShortAddress);
    OT_UNUSED_VARIABLE(aExtAddress);

    otError error = OT_ERROR_NONE;

    SuccessOrExit(error = otLinkMetricsConfigureEnhAckProbing(aShortAddress, aExtAddress,
                                                              aLinkMetrics));
    updateIeData(aInstance, aShortAddress, aExtAddress, pan_idx);

exit:
    return error;
}
#endif

otError otPlatRadioSetChannelMaxTransmitPower(otInstance *aInstance, uint8_t aChannel,
                                              int8_t aMaxPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    otError error = OT_ERROR_NONE;

    return error;
}

otError otPlatRadioSetRegion(otInstance *aInstance, uint16_t aRegionCode)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);

    radio_inst[pan_idx].sRegionCode = aRegionCode;
    return OT_ERROR_NONE;
}

otError otPlatRadioGetRegion(otInstance *aInstance, uint16_t *aRegionCode)
{
    uint8_t pan_idx = mpan_GetCurrentPANIdx();
    OT_UNUSED_VARIABLE(aInstance);
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aRegionCode != NULL, error = OT_ERROR_INVALID_ARGS);

    *aRegionCode = radio_inst[pan_idx].sRegionCode;
exit:
    return error;
}

APP_RAM_TEXT_SECTION uint8_t generate_ieee_enhack_frame(uint8_t *txbuf, uint8_t buf_len, fc_t *fc,
                                                        uint8_t seq,
                                                        uint16_t dpid, uint8_t *dadr, uint8_t aux_len, uint8_t *aux, uint8_t pan_idx)
{
    uint8_t len = 0;
    csl_ie_t *p_csl_ie;
    vendor_ie_t *p_vendor_ie;

    //frame control
    mac_memcpy((void *)(txbuf + len), (void *)fc, 2);
    len += 2;

    if (!fc->seq_num_suppress)
    {
        //sequence number
        txbuf[len] = seq;
        len += 1;
    }

    {
        if (!fc->panid_compress)
        {
            mac_memcpy(&txbuf[len], &dpid, 2);
            len += 2;
        }
        if (fc->dst_addr_mode > 0)
        {
            if (fc->dst_addr_mode == ADDR_MODE_SHORT)
            {
                mac_memcpy(&txbuf[len], dadr, 2);
                len += 2;
            }
            else
            {
                mac_memcpy(&txbuf[len], dadr, 8);
                len += 8;
            }
        }
    }

    if (fc->sec_en) //security check
    {
        mac_memcpy((void *)(txbuf + len), (void *)aux, aux_len);
        len += aux_len;
    }

    if (fc->ie_present) // ie
    {
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
        if (radio_inst[pan_idx].sCslPeriod > 0)
        {
            radio_inst[pan_idx].sCslIeIndex = len;
            p_csl_ie = (csl_ie_t *)&txbuf[radio_inst[pan_idx].sCslIeIndex];
            p_csl_ie->len = 4;
            p_csl_ie->id = 0x1a;
            p_csl_ie->type = 0;
            len += sizeof(csl_ie_t);
        }
        else
        {
            radio_inst[pan_idx].sCslIeIndex = 0;
        }
#endif
#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
        if (radio_inst[pan_idx].enhAckProbingDataLen > 0)
        {
            radio_inst[pan_idx].sVendorIeIndex = len;
            p_vendor_ie = (vendor_ie_t *)&txbuf[radio_inst[pan_idx].sVendorIeIndex];
            p_vendor_ie->len = 6;
            p_vendor_ie->id = 0;
            p_vendor_ie->type = 0;
            len += sizeof(vendor_ie_t);
        }
        else
        {
            radio_inst[pan_idx].sVendorIeIndex = 0;
        }
#endif
    }
    else
    {
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
        radio_inst[pan_idx].sCslIeIndex = 0;
#endif
#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
        radio_inst[pan_idx].sVendorIeIndex = 0;
#endif
    }

    return len;
}

#define TXNS_OFFSET         0
#define TXG1S_OFFSET        1
#define TXG2S_OFFSET        2
#define CCAFAIL_OFFSET      5
#define PTA_TX_FAIL_OFFSET  6

#define ACK_REQ_OFFSET  2
#define ACK_TYPE_OFFSET 3

APP_RAM_TEXT_SECTION void mac_report_pta_grant_failed(uint8_t pan_idx)
{
    radio_inst[pan_idx].tx_result = TX_PTA_FAIL;
    os_sem_give(radio_inst[pan_idx].radio_done);
}

APP_RAM_TEXT_SECTION void txnterr_handler(uint8_t pan_idx, uint32_t txat_status)
{
    radio_inst[pan_idx].tx_result = TX_AT_FAIL;
    os_sem_give(radio_inst[pan_idx].radio_done);
}

APP_RAM_TEXT_SECTION void txn_handler(uint8_t pan_idx, uint32_t txn_trig)
{
    uint8_t tx_status = mac_GetTxNStatus();
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
    uint32_t pta_counter = mac_GetPTACounter();
    if (pta_counter & 0xffff0000)
    {
        radio_inst[pan_idx].tx_result = TX_PTA_FAIL;
    }
    else
#endif
    {
        if (tx_status & (1 << CCAFAIL_OFFSET))
        {
            radio_inst[pan_idx].tx_result = TX_CCA_FAIL;
        }
        else
        {
            if (tx_status & (1 << TXNS_OFFSET))
            {
                if (mac_GetTxNTermedStatus())
                {
                    radio_inst[pan_idx].tx_result = TX_TERMED;
                }
                else
                {
                    radio_inst[pan_idx].tx_result = TX_NO_ACK;
                }
            }
            else
            {
                if (txn_trig & (1 << ACK_REQ_OFFSET))
                {
                    if (txn_trig & (1 << ACK_TYPE_OFFSET))
                    {
                    }
                    else
                    {
                        radio_inst[pan_idx].ack_fp = mac_GetImmAckPendingBit();
                        radio_inst[pan_idx].ack_receive_done = true;
                    }
                    radio_inst[pan_idx].tx_result = TX_WAIT_ACK;
                }
                else
                {
                    radio_inst[pan_idx].tx_result = TX_OK;
                }
            }
        }
    }
    os_sem_give(radio_inst[pan_idx].radio_done);
}

static void handleEnhAckTransmission(uint8_t pan_idx, uint64_t now, int8_t rssi, uint8_t lqi)
{
    uint32_t mhr_duration_us = 16 * 2 * PHY_SYMBOL_TIME;
    uint64_t ackPhrTxTime = now - 40 -
                            mhr_duration_us +
                            mac_RxFrameLength() * 2 * PHY_SYMBOL_TIME +
                            mac_GetEnhAckDelay() +
                            SHR_DURATION_US;

    BEE_tx_ack_started(&radio_inst[pan_idx].sEnhAckPsdu[1], rssi, lqi, pan_idx, (uint32_t)ackPhrTxTime);

    if (radio_inst[pan_idx].enhack_frm_sec_en)
    {
        mac_LoadTxEnhAckPayload(
            radio_inst[pan_idx].enhack_frm_len,
            radio_inst[pan_idx].enhack_frm_len,
            &radio_inst[pan_idx].sEnhAckPsdu[2]
        );
        txAckProcessSecurity(&radio_inst[pan_idx].sEnhAckPsdu[1], pan_idx);
    }
    else
    {
        mac_LoadTxEnhAckPayload(0, radio_inst[pan_idx].enhack_frm_len, &radio_inst[pan_idx].sEnhAckPsdu[2]);
    }

    if (MAC_STS_SUCCESS != mpan_TrigTxEnhAck(true, radio_inst[pan_idx].enhack_frm_sec_en, pan_idx))
    {
        otLogInfoPlat("txg1 gnt fail");
    }
    else
    {
        radio_inst[pan_idx].sStayAwake_b.waitAckTxDone = 1;
        startWaitAckTxDoneProtection(pan_idx,
                                     (ackPhrTxTime + mhr_duration_us + PHR_DURATION_US +
                                     (radio_inst[pan_idx].enhack_frm_len + 2) * 2 * PHY_SYMBOL_TIME),
                                     0);
    }
}

static void handleEnhAckTx(int pan_idx, uint64_t now)
{
    int8_t rssi = 0; // Default RSSI value
    uint8_t lqi = 0; // Default LQI value

#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    if (radio_inst[pan_idx].enhAckProbingDataLen > 0)
    {
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
        uint16_t rssi_raw;
        mac_GetRxBasebandReport(&rssi_raw, &lqi);
        rssi = mac_GetRSSIFromRaw(rssi_raw, mac_GetChannel());
#else
        mac_SetTxEnhAckPending(radio_inst[pan_idx].enhack_frm_len);
        return; // Early return to avoid calling handleEnhAckTransmission
#endif
    }
#endif
    handleEnhAckTransmission(pan_idx, now, rssi, lqi);
}

APP_RAM_TEXT_SECTION void rxely_handler(uint8_t pan_idx, uint32_t arg)
{
    fc_t fc_ack;
    struct
    {
        aux_sec_ctl_t sec_ctl;
        uint32_t frame_counter;
        uint8_t key_id;
    } __attribute__((packed)) aux;

    uint64_t now = otPlatTimeGet();
    if (mac_GetRxFrmAckReq())
    {
#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
        if (FRAME_VER_2015 == mac_GetRxFrmVersion())
        {
            uint16_t panid;
            uint16_t saddr;
            uint64_t laddr;

            radio_inst[pan_idx].enhack_frm_sec_en = mac_GetRxFrmSecEn();
            fc_ack.type = FRAME_TYPE_ACK;
            fc_ack.sec_en = radio_inst[pan_idx].enhack_frm_sec_en;
            if (mac_GetSrcMatchStatus())
            {
                mac_ClrSrcMatchStatus();
                fc_ack.pending = true;
            }
            else
            {
                fc_ack.pending = false;
            }
            fc_ack.ack_req = 0;
            fc_ack.panid_compress = mac_GetRxFrmPanidCompress();
            fc_ack.rsv = 0;
            fc_ack.seq_num_suppress = 0;
            fc_ack.ie_present = 0;
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
            if (radio_inst[pan_idx].sCslPeriod > 0) { fc_ack.ie_present = 1; }
#endif
#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
            if (radio_inst[pan_idx].enhAckProbingDataLen > 0) { fc_ack.ie_present = 1; }
#endif
            fc_ack.dst_addr_mode = mac_GetRxFrmSrcAddrMode();
            fc_ack.ver = FRAME_VER_2015;
            fc_ack.src_addr_mode = 0;

            if (radio_inst[pan_idx].enhack_frm_sec_en)
            {
                aux.sec_ctl.sec_level = mac_GetRxFrmSecLevel();
                aux.sec_ctl.key_id_mode = mac_GetRxFrmSecKeyIdMode();
                aux.sec_ctl.frame_counter_supp = 0;
                aux.sec_ctl.asn_in_nonce = 0;
                aux.sec_ctl.rsv = 0;
                // set frame counter
                aux.frame_counter = radio_inst[pan_idx].sMacFrameCounter;
                aux.key_id = mac_GetRxFrmSecKeyId();
            }

            panid = mpan_GetPANId(pan_idx);

            if (fc_ack.dst_addr_mode == ADDR_MODE_SHORT)
            {
                saddr = mac_GetRxFrmShortAddr();
                radio_inst[pan_idx].enhack_frm_len = generate_ieee_enhack_frame(
                                                         &radio_inst[pan_idx].sEnhAckPsdu[MAC_FRAME_TX_HDR_LEN],
                                                         IEEE802154_MAX_LENGTH, &fc_ack, mac_GetRxFrmSeq(), panid, (uint8_t *)&saddr, sizeof(aux),
                                                         (uint8_t *)&aux, pan_idx);
            }
            else
            {
                laddr = mac_GetRxFrmLongAddr();
                radio_inst[pan_idx].enhack_frm_len = generate_ieee_enhack_frame(
                                                         &radio_inst[pan_idx].sEnhAckPsdu[MAC_FRAME_TX_HDR_LEN],
                                                         IEEE802154_MAX_LENGTH, &fc_ack, mac_GetRxFrmSeq(), panid, (uint8_t *)&laddr, sizeof(aux),
                                                         (uint8_t *)&aux, pan_idx);
            }

            radio_inst[pan_idx].sEnhAckPsdu[0] = 0;
            radio_inst[pan_idx].sEnhAckPsdu[1] = radio_inst[pan_idx].enhack_frm_len + 2;

            handleEnhAckTx(pan_idx, now);
        }
        else
#endif
        {
            radio_inst[pan_idx].sStayAwake_b.waitAckTxDone = 1;
            // wait imm-ack tx done
            startWaitAckTxDoneProtection(pan_idx,
                                         (now + mac_RxFrameLength() * 2 * PHY_SYMBOL_TIME +
                                          200 + IMMACK_DURATION_US),
                                         0);
        }
    }
    while (0);
}

void mac_report_enhack_transmit_done(uint8_t pan_idx)
{
    uint8_t tx_status = mac_GetTxNStatus();

    radio_inst[pan_idx].enhack_frm_len = 0;
    if (radio_inst[pan_idx].enhack_frm_sec_en)
    {
        radio_inst[pan_idx].enhack_frm_sec_en = false;
    }

    if (tx_status & (1 << TXG1S_OFFSET))
    {
        if (mac_GetTxEnhAckTermedStatus())
        {
            otLogNotePlat("txg1 termed");
        }
        else
        {
            otLogNotePlat("txg1 fail %x", tx_status);
        }
    }

#if defined(WAIT_ACK_TX_DONE_EN) && (WAIT_ACK_TX_DONE_EN == 1)
    if (radio_inst[pan_idx].sWaitAckTxDoneTimer)
    {
        mac_SoftwareTimer_Stop(radio_inst[pan_idx].sWaitAckTxDoneTimer);
    }

    if (radio_inst[pan_idx].sStayAwake_b.waitAckTxDone)
    {
        radio_inst[pan_idx].sStayAwake_b.waitAckTxDone = 0;
    }
#endif

}

APP_RAM_TEXT_SECTION void rxdone_handler(uint8_t pan_idx, uint32_t arg)
{
    uint8_t frm_len;
    mac_rxfifo_tail_t *prx_fifo_tail;
    uint8_t channel;
    uint8_t bt_channel;
    int8_t rssi;
    uint8_t lqi;
    uint32_t frame_type = mac_GetRxFrmType();
#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
    uint8_t *buf = (uint8_t *)arg;
#else
    uint8_t *buf = NULL;
#endif

    if (FRAME_TYPE_ACK == frame_type)
    {
#if defined(RT_PLATFORM_BEE3PLUS)
        mac_Rx(radio_inst[pan_idx].ack_received.sReceivedPsdu);
#else
        mac_Rx(radio_inst[pan_idx].ack_item.sReceivedPsdu);
#endif
        radio_inst[pan_idx].ack_receive_done = true;
    }
    else
    {
        if (buf == NULL)
        {
            mac_Rx(radio_inst[pan_idx].rx_queue[radio_inst[pan_idx].rx_tail].sReceivedPsdu);
        }
        else
        {
            memcpy(radio_inst[pan_idx].rx_queue[radio_inst[pan_idx].rx_tail].sReceivedPsdu, buf, buf[0] + 8);
        }

        frm_len = radio_inst[pan_idx].rx_queue[radio_inst[pan_idx].rx_tail].sReceivedPsdu[0];
        prx_fifo_tail = (mac_rxfifo_tail_t *)
                        &radio_inst[pan_idx].rx_queue[radio_inst[pan_idx].rx_tail].sReceivedPsdu[1 + frm_len];
        channel = mpan_GetChannel(pan_idx);
        bt_channel = ((channel - 10) * 5) - 2; // 15.4 channel is started from channell (2405 MHz)
        rssi = mac_GetRSSIFromRaw(prx_fifo_tail->rssi, bt_channel);
        lqi = prx_fifo_tail->lqi;
        radio_inst[pan_idx].rx_queue[radio_inst[pan_idx].rx_tail].sReceivedChannel = channel;
        if (radio_inst[pan_idx].enhack_frm_len > 0 && mac_GetTxEnhAckPending())
        {
            uint64_t ackPhrTxTime = bt_clk_offset +
                                    mac_BTClkToUS(prx_fifo_tail->bt_time) + frm_len * 2 * PHY_SYMBOL_TIME +
                                    mac_GetEnhAckDelay() + SHR_DURATION_US;
            BEE_tx_ack_started(&radio_inst[pan_idx].sEnhAckPsdu[1], rssi, lqi, pan_idx, (uint32_t)ackPhrTxTime);
            if (radio_inst[pan_idx].enhack_frm_sec_en)
            {
                mac_LoadTxEnhAckPayload(radio_inst[pan_idx].enhack_frm_len, radio_inst[pan_idx].enhack_frm_len,
                                        &radio_inst[pan_idx].sEnhAckPsdu[2]);
                txAckProcessSecurity(&radio_inst[pan_idx].sEnhAckPsdu[1], pan_idx);
            }
            else
            {
                mac_LoadTxEnhAckPayload(0, radio_inst[pan_idx].enhack_frm_len, &radio_inst[pan_idx].sEnhAckPsdu[2]);
            }
            if (MAC_STS_SUCCESS != mpan_TrigTxEnhAck(false, radio_inst[pan_idx].enhack_frm_sec_en, pan_idx))
            {
                otLogInfoPlat("txg1 gnt fail");
            }
#if defined(DLPS_EN) && (DLPS_EN == 1)
            else
            {
                radio_inst[pan_idx].sStayAwake_b.waitAckTxDone = 1;
                startWaitAckTxDoneProtection(pan_idx,
                                             (ackPhrTxTime + PHR_DURATION_US +
                                              (radio_inst[pan_idx].enhack_frm_len + 2) * 2 * PHY_SYMBOL_TIME),
                                             64);
                //otLogNotePlat("enhack trigger ok");
            }
#endif
        }

        radio_inst[pan_idx].rx_tail = (radio_inst[pan_idx].rx_tail + 1) % RX_BUF_SIZE;
        if (pan_idx == 0) { otSysEventSignalPending(); }
        else { zbSysEventSignalPending(); }
    }
}

APP_RAM_TEXT_SECTION void handle_tx_backoff_pending(uint8_t pan_idx)
{
#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_US_ALARM) || (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_MS_ALARM)
    if (radio_inst[pan_idx].tx_backoff_pending)
    {
        uint32_t now_btus;
        uint64_t now;
        now = otPlatTimeGetWithBTUS(&now_btus);
        if (radio_inst[pan_idx].tx_backoff_pending < now + 20)
        {
            radio_inst[pan_idx].tx_backoff_pending = 0;
            radio_inst[pan_idx].tx_backoff_tmo = true;
            if (pan_idx == 0) { otSysEventSignalPending(); }
            else { zbSysEventSignalPending(); }
        }
        else
        {
            uint32_t diff = radio_inst[pan_idx].tx_backoff_pending - now;
#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_US_ALARM)
            mac_SetBTClkUSInt((pan_idx == 0) ? MAC_BT_TIMER1 : MAC_BT_TIMER5, now_btus + diff);
#elif (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_MS_ALARM)
            mac_SetBTClkUSInt((pan_idx == 0) ? MAC_BT_TIMER0 : MAC_BT_TIMER4, now_btus + diff);
#endif
        }
    }
#endif
}

APP_RAM_TEXT_SECTION void btcmp0_handler(uint8_t pan_idx, uint32_t arg)
{
#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_MS_ALARM)
    handle_tx_backoff_pending(0);
#endif
    milli_handler(0);
}

APP_RAM_TEXT_SECTION void btcmp1_handler(uint8_t pan_idx, uint32_t arg)
{
#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_US_ALARM)
    handle_tx_backoff_pending(0);
#endif
    micro_handler(0);
}

APP_RAM_TEXT_SECTION void edscan_handler(uint8_t pan_idx, uint32_t arg)
{
    os_sem_give(radio_inst[pan_idx].radio_done);
}

#if defined(RT_PLATFORM_BB2ULTRA) || defined(RT_PLATFORM_RTL8922D)
APP_RAM_TEXT_SECTION void btcmp4_handler(uint8_t pan_idx, uint32_t arg)
{
#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_MS_ALARM)
    handle_tx_backoff_pending(1);
#endif
    milli_handler(1);
}

APP_RAM_TEXT_SECTION void btcmp5_handler(uint8_t pan_idx, uint32_t arg)
{
#if (TX_BACKOFF_TMR_MODE == TX_BACKOFF_USE_US_ALARM)
    handle_tx_backoff_pending(1);
#endif
    micro_handler(1);
}
#endif
