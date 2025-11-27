/*
 *  Copyright (c) 2016, The OpenThread Authors.
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
 *   This file includes Sbee2 compile-time configuration constants for OpenThread.
 */

#ifndef OPENTHREAD_CORE_SBEE2_CONFIG_H_
#define OPENTHREAD_CORE_SBEE2_CONFIG_H_

/**
 * @def OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS
 *
 */
#ifndef OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS
#define OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS 160
#endif

/**
 * @def OPENTHREAD_CONFIG_LOG_LEVEL
 *
 */
#ifndef OPENTHREAD_CONFIG_LOG_LEVEL
#define OPENTHREAD_CONFIG_LOG_LEVEL OT_LOG_LEVEL_NOTE
#endif

/**
 * @def OPENTHREAD_CONFIG_LOG_OUTPUT
 *
 */
#define OPENTHREAD_CONFIG_LOG_OUTPUT OPENTHREAD_CONFIG_LOG_OUTPUT_PLATFORM_DEFINED

/**
 * @def OPENTHREAD_CONFIG_PLATFORM_INFO
 *
 * The platform-specific string to insert into the OpenThread version string.
 *
 */
#define OPENTHREAD_CONFIG_PLATFORM_INFO "BEE4"

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_ACK_TIMEOUT_ENABLE
 *
 * Define to 1 if you want to enable software ACK timeout logic.
 *
 */
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_ACK_TIMEOUT_ENABLE 0

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_RETRANSMIT_ENABLE
 *
 * Define to 1 if you want to enable software retransmission logic.
 *
 */
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_RETRANSMIT_ENABLE 1

/**
 * @def OPENTHREAD_CONFIG_MAC_ADD_DELAY_ON_NO_ACK_ERROR_BEFORE_RETRY
 *
 * Define as 1 to add random backoff delay in between frame transmission retries when the previous attempt resulted in
 * no-ack error.
 *
 */
#define OPENTHREAD_CONFIG_MAC_ADD_DELAY_ON_NO_ACK_ERROR_BEFORE_RETRY 0

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE
 *
 * Define to 1 if you want to enable software CSMA-CA backoff logic.
 *
 */
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE 0

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_SECURITY_ENABLE
 *
 * Define to 1 if you want to enable software transmission security logic.
 *
 */
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_SECURITY_ENABLE 0

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_ENERGY_SCAN_ENABLE
 *
 * Define to 1 if you want to enable software energy scanning logic.
 *
 */
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_ENERGY_SCAN_ENABLE 0

/**
 * @def OPENTHREAD_CONFIG_NCP_HDLC_ENABLE
 *
 * Define to 1 to enable NCP HDLC support.
 *
 */
#define OPENTHREAD_CONFIG_NCP_HDLC_ENABLE 1

/**
 * @def OPENTHREAD_CONFIG_PLATFORM_FLASH_API_ENABLE
 *
 * Define to 1 to enable otPlatFlash* APIs to support non-volatile storage.
 *
 * When defined to 1, the platform MUST implement the otPlatFlash* APIs instead of the otPlatSettings* APIs.
 *
 */
#define OPENTHREAD_CONFIG_PLATFORM_FLASH_API_ENABLE 1

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE
 *
 * Define to 1 to enable software transmission target time logic.
 *
 */
#ifndef OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
#endif

/**
 * @def OPENTHREAD_CONFIG_MAC_SOFTWARE_RX_TIMING_ENABLE
 *
 * Define to 1 to enable software reception target time logic.
 *
 */
#ifndef OPENTHREAD_CONFIG_MAC_SOFTWARE_RX_TIMING_ENABLE
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_RX_TIMING_ENABLE (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
#endif

/**
 * @def OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
 *
 * Define to 1 if you want to support microsecond timer in platform.
 *
 */
#ifndef OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
#define OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE 1
#endif

/**
 * @def RADIO_CONFIG_SRC_MATCH_SHORT_ENTRY_NUM
 *
 * The number of short source address table entries.
 *
 */
#ifndef RADIO_CONFIG_SRC_MATCH_SHORT_ENTRY_NUM
#define RADIO_CONFIG_SRC_MATCH_SHORT_ENTRY_NUM 0
#endif

/**
 * @def RADIO_CONFIG_SRC_MATCH_EXT_ENTRY_NUM
 *
 * The number of extended source address table entries.
 *
 */
#ifndef RADIO_CONFIG_SRC_MATCH_EXT_ENTRY_NUM
#define RADIO_CONFIG_SRC_MATCH_EXT_ENTRY_NUM 0
#endif

/**
 * @def OPENTHREAD_CONFIG_CSL_RECEIVE_TIME_AHEAD
 *
 * Reception scheduling and ramp up time needed for the CSL receiver to be ready, in units of microseconds.
 *
 * @note The value was reduced from 2000 to 300 microseconds (85% reduction).
 *
 * Rationale:
 * - Hardware and platform measurements indicate that radio ramp-up and scheduling jitter
 *   are consistently below 300us.
 * - The previous value (2000us) was overly conservative for this platform.
 * - Testing has shown that 300us provides sufficient margin for reliable CSL operation,
 *   even accounting for clock drift and interrupt latency.
 *
 * Benefits:
 * - Improves CSL timing precision by reducing unnecessary early wake-up margin.
 * - Reduces energy consumption by minimizing the time the receiver is active before
 *   the expected frame arrival, extending device battery life.
 * - CSL operation remains robust, with no observed increase in missed packets or
 *   timing-related errors.
 *
 * @note If hardware or timing conditions change, this value should be re-evaluated.
 */
#ifndef OPENTHREAD_CONFIG_CSL_RECEIVE_TIME_AHEAD
#define OPENTHREAD_CONFIG_CSL_RECEIVE_TIME_AHEAD 300
#endif

/**
 * @def OPENTHREAD_CONFIG_CSL_MIN_RECEIVE_ON
 *
 * The minimum CSL receive window (in microseconds) required to receive an IEEE 802.15.4 frame.
 * - Frame preamble: 6*2 symbols + margin
 *
 */
#ifndef OPENTHREAD_CONFIG_MIN_RECEIVE_ON_AHEAD
#define OPENTHREAD_CONFIG_MIN_RECEIVE_ON_AHEAD 12 * 16
#endif

/**
 * @def OPENTHREAD_CONFIG_MIN_RECEIVE_ON_AFTER
 *
 * The minimum time (in microseconds) after the MHR start that the radio should be in receive state in order
 * to properly receive any IEEE 802.15.4 frame. Defaults to the duration of a maximum size frame, plus AIFS,
 * plus the duration of maximum enh-ack frame. Platforms are encouraged to improve this value for energy
 * efficiency purposes.
 *
 */
#ifndef OPENTHREAD_CONFIG_MIN_RECEIVE_ON_AFTER
#define OPENTHREAD_CONFIG_MIN_RECEIVE_ON_AFTER 0
#endif

#ifndef OPENTHREAD_CONFIG_CLI_MAX_USER_CMD_ENTRIES
#define OPENTHREAD_CONFIG_CLI_MAX_USER_CMD_ENTRIES 2
#endif

#endif // OPENTHREAD_CORE_SBEE2_CONFIG_H_

