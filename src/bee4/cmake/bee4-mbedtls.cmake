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

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR 
   ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd" OR
   ${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-cli-ftd" OR
   ${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-cli-mtd")

    message("CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

    set(MBEDTLS_REPO ${OT_REALTEK_ROOT}/third_party/Realtek/rtl87x2g_sdk/subsys/mbedtls/repo)
    set(MBEDTLS_COMMON_CONFIG ${OT_REALTEK_ROOT}/third_party/Realtek/rtl87x2g_sdk/subsys/mbedtls/mbedtls-config.h)


    add_subdirectory(
        ${MBEDTLS_REPO}
        ${CMAKE_CURRENT_BINARY_DIR}/mbedtls)


    target_compile_definitions(ot-config INTERFACE
        MBEDTLS_USER_CONFIG_FILE="${RT_PLATFORM}-mbedtls-config.h"
    )


    target_include_directories(ot-config SYSTEM
        INTERFACE
            ${CMAKE_CURRENT_BINARY_DIR}
            ${MBEDTLS_REPO}/include)

    target_compile_definitions(mbedtls
        PUBLIC
            "MBEDTLS_CONFIG_FILE=\"${MBEDTLS_COMMON_CONFIG}\""
            "MBEDTLS_ALLOW_PRIVATE_ACCESS="
        PRIVATE
            $<TARGET_PROPERTY:ot-config,INTERFACE_COMPILE_DEFINITIONS>
    )

    target_include_directories(mbedtls
        PUBLIC
            $<BUILD_INTERFACE:${MBEDTLS_REPO}/include>
        PRIVATE
            ${OT_PUBLIC_INCLUDES}
            $<TARGET_PROPERTY:ot-config,INTERFACE_INCLUDE_DIRECTORIES>
    )

    target_compile_definitions(mbedx509
        PUBLIC
            "MBEDTLS_CONFIG_FILE=\"${MBEDTLS_COMMON_CONFIG}\""
            "MBEDTLS_ALLOW_PRIVATE_ACCESS="
        PRIVATE
            $<TARGET_PROPERTY:ot-config,INTERFACE_COMPILE_DEFINITIONS>

    )
    target_include_directories(mbedx509
        PUBLIC
            $<BUILD_INTERFACE:${MBEDTLS_REPO}/include>
        PRIVATE
            ${OT_PUBLIC_INCLUDES}
            $<TARGET_PROPERTY:ot-config,INTERFACE_INCLUDE_DIRECTORIES>
    )

    target_compile_definitions(mbedcrypto
        PUBLIC
            "MBEDTLS_CONFIG_FILE=\"${MBEDTLS_COMMON_CONFIG}\""
            "MBEDTLS_ALLOW_PRIVATE_ACCESS="
        PRIVATE
            $<TARGET_PROPERTY:ot-config,INTERFACE_COMPILE_DEFINITIONS>
    )
    target_include_directories(mbedcrypto
        PUBLIC
            $<BUILD_INTERFACE:${MBEDTLS_REPO}/include>
        PRIVATE
            ${OT_PUBLIC_INCLUDES}
            $<TARGET_PROPERTY:ot-config,INTERFACE_INCLUDE_DIRECTORIES>
    )

    target_compile_options(mbedcrypto PRIVATE
        -include "${OT_REALTEK_ROOT}/third_party/Realtek/rtl87x2g_sdk/subsys/crypto/mbedtls/mbedtls_lib/src/mbedtls_hw_interface.h"
    )

else()

    if (NOT OT_EXTERNAL_MBEDTLS)
        if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-ncp-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-ncp-mtd")
            target_compile_definitions(ot-config INTERFACE
                    MBEDTLS_USER_CONFIG_FILE="ncp-mbedtls-config.h"
                    MBEDTLS_ALLOW_PRIVATE_ACCESS=
            )
        else()
            target_compile_definitions(ot-config INTERFACE
                    MBEDTLS_USER_CONFIG_FILE="${RT_PLATFORM}-mbedtls-config.h"
            )
        endif()
    else()

        add_library(bee4-mbedtls INTERFACE)

    target_link_libraries(bee4-mbedtls
        INTERFACE
            ${OT_REALTEK_ROOT}/lib/bee4/libmbedtls.a
            ${OT_REALTEK_ROOT}/lib/bee4/libmbedx509.a
            ${OT_REALTEK_ROOT}/lib/bee4/libmbedcrypto.a
    )

        target_include_directories(ot-config
            INTERFACE
                ${REALTEK_SDK_ROOT}/subsys/mbedtls
                ${REALTEK_SDK_ROOT}/subsys/mbedtls/repo/include
        )

        target_compile_definitions(ot-config INTERFACE
                MBEDTLS_CONFIG_FILE="mbedtls-config.h"
                MBEDTLS_USER_CONFIG_FILE="${RT_PLATFORM}-mbedtls-config.h"
                MBEDTLS_ALLOW_PRIVATE_ACCESS=
        )

    endif()

endif()
