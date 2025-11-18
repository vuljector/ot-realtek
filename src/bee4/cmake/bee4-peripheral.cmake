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

add_library(bee4-peripheral
    # peripheral
    "${REALTEK_SDK_ROOT}/bsp/driver/pinmux/src/rtl87x2g/rtl_pinmux.c"
    "${REALTEK_SDK_ROOT}/bsp/driver/rcc/src/rtl87x2g/rtl_rcc.c"
    "${REALTEK_SDK_ROOT}/bsp/driver/nvic/src/rtl87x2g/rtl_nvic.c"
    "${REALTEK_SDK_ROOT}/bsp/driver/tim/src/rtl_common/rtl_tim.c"
    "${REALTEK_SDK_ROOT}/bsp/driver/uart/src/rtl_common/rtl_uart.c"
    "${REALTEK_SDK_ROOT}/bsp/driver/dma/src/rtl_common/rtl_gdma.c"
    "${REALTEK_SDK_ROOT}/bsp/driver/gpio/src/rtl_common/rtl_gpio.c"
)

set_target_properties(
    bee4-peripheral
    PROPERTIES
        C_STANDARD 99
)

target_link_directories(bee4-peripheral
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_TARGET}
        ${PROJECT_SOURCE_DIR}/lib/${RT_PLATFORM}
)

target_include_directories(bee4-peripheral
    PRIVATE
        ${REALTEK_SDK_INCPATH}
)

