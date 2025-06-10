#
#  Copyright (c) 2020, The OpenThread Authors.
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

cmake_minimum_required(VERSION 3.10.2)
project(ot-bee VERSION 0.2.0 LANGUAGES C CXX ASM)

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd")
set(OT_EXTERNAL_MBEDTLS "bee4-mbedtls" CACHE STRING "")

if(${BUILD_TARGET} STREQUAL "rtl8777g")
set(RT_PLATFORM "bee4" CACHE STRING "")
endif()

set(bee_matter_root "${MATTER_EXAMPLE_PATH}/../../../..")

set(ENV{MATTER_ENABLE_MED} 0)
set(matter_enable_factory_data 0)
if(matter_enable_factory_data GREATER 0)
    set(matter_enable_factory_data 1)
    set(matter_dac_key_encryption 1)
endif()
set (matter_enable_persistentstorage_audit 0)
set (matter_enable_ota_requestor 0)
set (matter_enable_ble_ota 0)

if(${ENABLE_PW_RPC} STREQUAL "ON")
    set (matter_enable_rpc 1)
else()
    set (matter_enable_rpc 0)
endif()

if(${ENABLE_CLI} STREQUAL "ON")
    set(matter_enable_otcli 1)
else()
    set(matter_enable_otcli 0)
endif()

if(${ENABLE_SHELL} STREQUAL "ON")
    set(matter_enable_shell 1)
else()
    set(matter_enable_shell 0)
endif()

endif()

set(RT_PLATFORM_VALUES
    "bee4"
    "bee3plus"
    "rtl8852d"
    "rtl8922d"
    "bb2ultra"
)
set_property(CACHE RT_PLATFORM PROPERTY STRINGS ${RT_PLATFORM_VALUES})
if(NOT RT_PLATFORM IN_LIST RT_PLATFORM_VALUES)
    message(FATAL_ERROR "Please select a supported platform: ${RT_PLATFORM_VALUES}")
endif()

# OpenThread config
if(${RT_PLATFORM} STREQUAL "bee3plus" AND ${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-cli-mtd")
set(OT_PLATFORM "external" CACHE STRING "")
set(OT_SLAAC ON CACHE BOOL "")
set(OT_LOG_LEVEL_DYNAMIC ON CACHE BOOL "")
set(OT_CSL_RECEIVER ON CACHE BOOL "")
set(OT_CHANNEL_MONITOR OFF CACHE BOOL "")
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
endif()

set(OT_PLATFORM_LIB "openthread-${RT_PLATFORM}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(openthread)
include(script/${BUILD_TYPE}.cmake)

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
    ${PROJECT_SOURCE_DIR}/openthread/examples/platforms
    ${PROJECT_SOURCE_DIR}/src/${RT_PLATFORM}
    ${REALTEK_SDK_INCPATH}
)

add_subdirectory(src/${RT_PLATFORM})
if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd")
set (matter_output_path ${PROJECT_BINARY_DIR})
set (matter_example_path ${MATTER_EXAMPLE_PATH})
get_filename_component(SDK_ROOT ${REALTEK_SDK_ROOT} REALPATH)
get_filename_component(CHIP_ROOT ${bee_matter_root} REALPATH)

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd")
set (matter_enable_ftd 1)
endif()

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd")
set (matter_enable_mtd 1)
set (matter_enable_med $ENV{MATTER_ENABLE_MED})
endif()

set (openthread_root "${CMAKE_CURRENT_SOURCE_DIR}/openthread")

set(bee_lwip_path ${REALTEK_SDK_ROOT}/subsys/lwip/for_matter)
include(${bee_lwip_path}/lwip.cmake)

include(includepath.cmake)
include(${CHIP_ROOT}/src/app/chip_data_model.cmake)
include(${matter_example_path}/chip_main.cmake)
include(${CHIP_ROOT}/config/realtek/bee/chip.cmake)

add_dependencies(chip_main chip-gn)
endif()
