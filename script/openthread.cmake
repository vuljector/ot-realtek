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

# OpenThread config

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd")
    # config for matter have been moved to matter repository
elseif(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-ncp-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-ncp-mtd")
    set(OT_PLATFORM "external" CACHE STRING "")
else()
    set(OT_PLATFORM "external" CACHE STRING "")
    set(OT_SLAAC ON CACHE BOOL "")
    set(OT_BORDER_AGENT ON CACHE BOOL "")
    set(OT_BORDER_ROUTER ON CACHE BOOL "")
    set(OT_COMMISSIONER ON CACHE BOOL "")
    set(OT_JOINER ON CACHE BOOL "")
    set(OT_DNS_CLIENT ON CACHE BOOL "")
    set(OT_DHCP6_SERVER ON CACHE BOOL "")
    set(OT_DHCP6_CLIENT ON CACHE BOOL "")
    set(OT_COAP ON CACHE BOOL "")
    set(OT_COAPS ON CACHE BOOL "")
    set(OT_COAP_OBSERVE ON CACHE BOOL "")
    set(OT_LINK_RAW ON CACHE BOOL "")
    set(OT_MAC_FILTER ON CACHE BOOL "")
    set(OT_SERVICE ON CACHE BOOL "")
    set(OT_UDP_FORWARD ON CACHE BOOL "")
    set(OT_ECDSA ON CACHE BOOL "")
    set(OT_SNTP_CLIENT ON CACHE BOOL "")
    set(OT_CHILD_SUPERVISION ON CACHE BOOL "")
    set(OT_JAM_DETECTION ON CACHE BOOL "")
    set(OT_LOG_LEVEL_DYNAMIC ON CACHE BOOL "")
    set(OT_EXTERNAL_HEAP ON CACHE BOOL "")
    set(OT_SRP_CLIENT ON CACHE BOOL "")
    set(OT_DUA ON CACHE BOOL "")
    set(OT_BACKBONE_ROUTER ON CACHE BOOL "")
    set(OT_MLR ON CACHE BOOL "")
    set(OT_CSL_RECEIVER ON CACHE BOOL "")
    set(OT_LINK_METRICS_INITIATOR ON CACHE BOOL "")
    set(OT_LINK_METRICS_SUBJECT ON CACHE BOOL "")
    set(OT_CHANNEL_MONITOR OFF CACHE BOOL "")
    set(OT_CHANNEL_MANAGER OFF CACHE BOOL "")
if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-cli-mtd")
    set(OT_EXTERNAL_MBEDTLS "mbedtls" CACHE STRING "")
endif()
endif()


set(OT_PLATFORM_LIB "openthread-${RT_PLATFORM}")

add_subdirectory(${OT_REALTEK_ROOT}/openthread ${CMAKE_CURRENT_BINARY_DIR}/openthread)

target_compile_definitions(ot-config INTERFACE
    OPENTHREAD_CONFIG_FILE="openthread-core-${RT_PLATFORM}-config.h"
    OPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE="openthread-core-${RT_PLATFORM}-config-check.h"
)

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-rcp")
    target_compile_definitions(ot-config INTERFACE
        OPENTHREAD_PROJECT_CORE_CONFIG_FILE="openthread-core-${BUILD_TARGET}-rcp-config.h"
    )
else()
    target_compile_definitions(ot-config INTERFACE
        OPENTHREAD_PROJECT_CORE_CONFIG_FILE="openthread-core-${RT_PLATFORM}-config.h"
    )
endif()

target_include_directories(ot-config INTERFACE
    ${OT_REALTEK_ROOT}/openthread/examples/platforms
    ${OT_REALTEK_ROOT}/src/${RT_PLATFORM}
    ${REALTEK_SDK_INCPATH}
)
