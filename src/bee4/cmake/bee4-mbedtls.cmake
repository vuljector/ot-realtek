

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
