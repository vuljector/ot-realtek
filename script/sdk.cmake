#
#  Copyright (c) 2025, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

if(${RT_PLATFORM} STREQUAL "rtl8852d")
    include(${PROJECT_SOURCE_DIR}/cmake/fetch_openthread.cmake)
endif()

if(${RT_PLATFORM} STREQUAL "bee4")
    set(REALTEK_SDK_ROOT
        ${OT_REALTEK_ROOT}/third_party/Realtek/rtl87x2g_sdk
    )
    set(REALTEK_SDK_INCPATH
        ${REALTEK_SDK_ROOT}/subsys/freertos
        ${REALTEK_SDK_ROOT}/subsys/osif/inc
        ${REALTEK_SDK_ROOT}/subsys/bluetooth/bt_host/inc
        ${REALTEK_SDK_ROOT}/include/rtl87x2g
        ${REALTEK_SDK_ROOT}/include/rtl87x2g/nsc
        ${REALTEK_SDK_ROOT}/include/rtl87x2g/cmsis/Core/Include
        ${REALTEK_SDK_ROOT}/bsp/driver/
        ${REALTEK_SDK_ROOT}/bsp/driver/pinmux/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/rcc/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/nvic/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/uart/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/dma/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/gpio/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/spi/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/i2c/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/tim/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/wdt/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/ir/inc
        ${REALTEK_SDK_ROOT}/bsp/driver/project/rtl87x2g/inc
        ${REALTEK_SDK_ROOT}/bsp/sdk_lib/inc
        ${REALTEK_SDK_ROOT}/bsp/sdk_lib/inc_int
        ${REALTEK_SDK_ROOT}/bsp/power
    )
endif()
