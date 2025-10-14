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
set(TARGETS_CONFIGS
    "evb"                      "-DBOARD_EVB"                          "app.ld.gen"
    "evb_dual"                 "-DBOARD_EVB"                          "app.ld.gen"
    "rtl8771guv"               "-DBOARD_RTL8771GUV -DBUILD_USB"       "app.ld"
    "rtl8771gtv"               "-DBOARD_RTL8771GTV"                   "app.ld"
    "rtl8777g"                 "-DBOARD_RTL8777G"                     "app.ld.gen"
    "rtl8777g_cert"            "-DBOARD_RTL8777G -DBUILD_CERT"        "app.ld.gen"
    "rtl8777g_test"            "-DBOARD_RTL8777G"                     "app.ld.gen"
)

function(setup_target target_name)
    list(FIND TARGETS_CONFIGS "${target_name}" index)
    if(NOT index LESS 0)
        math(EXPR flag_index "${index} + 1")
        math(EXPR ld_file_index "${index} + 2")

        list(GET TARGETS_CONFIGS ${flag_index} board_flags)
        list(GET TARGETS_CONFIGS ${ld_file_index} ld_file_suffix)

        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${board_flags}" PARENT_SCOPE)
        set(LD_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_TARGET}/${ld_file_suffix}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown build board target: ${target_name}")
    endif()
endfunction()

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd")
    message(STATUS "Build matter. Skip build board target.")
else()
    setup_target(${BUILD_BOARD_TARGET})
endif()

include(cmake/bee4-internal.cmake)
include(cmake/bee4-mbedtls.cmake)
include(cmake/bee4-peripheral.cmake)
include(cmake/openthread-bee4.cmake)

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd")
    include(${MATTER_ADAPTER_PATH}/vendor/matter_adapter.cmake)
endif()
