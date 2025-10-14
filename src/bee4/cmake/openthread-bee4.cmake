add_library(openthread-bee4
    ../alarm.c
    ../diag.c
    ../entropy.c
    flash.c
    logging.c
    ../misc.c
    ../radio.c
    system.c
    uart.c
    # start up and entry point
    "${REALTEK_SDK_ROOT}/bsp/boot/rtl87x2g/startup_rtl.c"
    system_rtl.c
    zb_main.c
    thread_task.c
    # crypto
    # app
    xmodem.c
    dbg_printf.c
    # profile
    # peripheral
    # utils
    "${OT_REALTEK_ROOT}/openthread/examples/platforms/utils/mac_frame.cpp"
    "${OT_REALTEK_ROOT}/openthread/examples/platforms/utils/link_metrics.cpp"
    ../mac_frame_add.cpp
    ../mac_802154_frame_parser.c
)

set_target_properties(
    openthread-bee4
    PROPERTIES
        C_STANDARD 99
        CXX_STANDARD 11
)

string(REGEX REPLACE "(/.*|_.*$)" "" BUILD_TARGET_VALID "${BUILD_TARGET}")

target_link_directories(openthread-bee4
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_TARGET}
        ${OT_REALTEK_ROOT}/lib/${RT_PLATFORM}/${BUILD_TARGET_VALID}
        ${REALTEK_SDK_ROOT}/lib/${RT_PLATFORM}/${BUILD_TARGET_VALID}
        ${REALTEK_SDK_ROOT}/subsys/lwip/for_matter/lib
)

target_link_libraries(openthread-bee4
    PRIVATE
        ${OT_MBEDTLS}
        ot-config
        bee4-internal
        bee4-peripheral
        "${REALTEK_SDK_ROOT}/subsys/usb/usb_lib/lib/rtl87x2g/gcc/libusb.a"
        "${REALTEK_SDK_ROOT}/subsys/usb/usb_hal/lib/rtl87x2g/gcc/libusb_hal.a"
        "${REALTEK_SDK_ROOT}/subsys/bluetooth/gap_ext/lib/rtl87x2g/bt_host_0_0/gcc/libgap_utils.a"
        "${REALTEK_SDK_ROOT}/subsys/mac_driver/portable/bee4/bee4-internal.axf"
        "${REALTEK_SDK_ROOT}/bsp/driver/driver_lib/lib/rtl87x2g/gcc/librtl87x2g_io.a"
        "${REALTEK_SDK_ROOT}/bsp/sdk_lib/lib/rtl87x2g/gcc/librtl87x2g_sdk.a"
        "${REALTEK_SDK_ROOT}/bin/rtl87x2g/rom_lib/libROM_NS.a"
        "${REALTEK_SDK_ROOT}/bin/rtl87x2g/rom_lib/ROM_CMSE_Lib.o"
        "${REALTEK_SDK_ROOT}/bin/rtl87x2g/rom_lib/liblowerstack.a"
)

target_link_options(openthread-bee4
    PUBLIC
        -T${LD_FILE}
        -Wl,--no-wchar-size-warning
        -Wl,--wrap,_malloc_r
        -Wl,--wrap,_free_r
        -Wl,--wrap,_realloc_r
        -Wl,--wrap,_calloc_r
        -Wl,--wrap,memcpy
        -Wl,--wrap,memset
        -Wl,--wrap,snprintf
        -Wl,--wrap,vsnprintf
        -Wl,--wrap,mac_EnterCritical_rom
        -Wl,--wrap,mac_ExitCritical_rom
        -Wl,--wrap,mac_RstRF_rom
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        -Wl,--no-warn-rwx-segments
)

target_compile_definitions(openthread-bee4
    PUBLIC
        ${OT_PLATFORM_DEFINES}
        "BUILD_WITHOUT_FTL=1"
        "RT_PLATFORM_BEE4"
)

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-ncp-ftd")
    target_compile_definitions(openthread-bee4 PUBLIC "BUILD_NCP=1")
endif()

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-ncp-mtd")
    target_compile_definitions(openthread-bee4 PUBLIC "BUILD_NCP=1")
endif()

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-rcp")
    target_compile_definitions(openthread-bee4 PUBLIC "BUILD_RCP=1")
endif()


if(NOT DEFINED BUILD_TRANSMIT_RETRY)
    set(BUILD_TRANSMIT_RETRY "ON")
endif()

if(${BUILD_TRANSMIT_RETRY} STREQUAL "ON")
    target_compile_definitions(openthread-bee4 PUBLIC "BUILD_TRANSMIT_RETRY=1")
endif()

target_compile_options(openthread-bee4
    PRIVATE
        ${OT_CFLAGS}
        -Wno-attributes
)

target_include_directories(openthread-bee4
    PRIVATE
        ${OT_PUBLIC_INCLUDES}
        ${OT_REALTEK_ROOT}/src/core
        ${OT_REALTEK_ROOT}/openthread/examples/platforms
        ${OT_REALTEK_ROOT}/src
        ${REALTEK_SDK_INCPATH}
        ${REALTEK_SDK_ROOT}/subsys/mac_driver
        ${REALTEK_SDK_ROOT}/subsys/mac_driver/portable/bee4
        ${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_TARGET}
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${REALTEK_SDK_ROOT}/subsys/usb/usb_hal/inc
        ${REALTEK_SDK_ROOT}/subsys/usb/usb_lib/inc/class
        ${REALTEK_SDK_ROOT}/subsys/usb/usb_lib/inc/composite
)

if(${BUILD_TYPE} STREQUAL "dev")
    add_custom_command(
        TARGET openthread-bee4
        POST_BUILD
        COMMAND mkdir -p ${OT_REALTEK_ROOT}/lib/${RT_PLATFORM}/${BUILD_TARGET} && cp -f ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libbee4-internal.a ${OT_REALTEK_ROOT}/lib/${RT_PLATFORM}/${BUILD_TARGET}/
        COMMAND cp -f ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmbedcrypto.a ${OT_REALTEK_ROOT}/lib/${RT_PLATFORM}/
        COMMAND cp -f ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmbedtls.a ${OT_REALTEK_ROOT}/lib/${RT_PLATFORM}/
        COMMAND cp -f ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmbedx509.a ${OT_REALTEK_ROOT}/lib/${RT_PLATFORM}/
        COMMAND cd ${OT_REALTEK_ROOT}/openthread && git reset --hard HEAD && git clean -f -d
    )
endif()

##################################################
# ot-cli-ftd or ot-ncp-ftd
##################################################
if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-cli-ftd")
    target_sources(ot-cli-ftd
        PRIVATE
        ${OT_REALTEK_ROOT}/src/bee4/main_ns.c
    )

    target_include_directories(ot-cli-ftd
        PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/${BUILD_TARGET}
    )

    target_compile_definitions(ot-cli-ftd
        PRIVATE
            "DLPS_EN=0"
    )

    if(${BUILD_BOARD_TARGET} STREQUAL "rtl8777g_test")
        target_compile_definitions(openthread-bee4
            PRIVATE
                "BUILD_CERT"
                "XMODEM_ENABLE=1"
        )
    endif()
endif()

if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-ncp-ftd")
    target_sources(ot-ncp-ftd
        PRIVATE
        ${OT_REALTEK_ROOT}/src/bee4/main_ns.c
    )

    target_include_directories(ot-ncp-ftd
        PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/${BUILD_TARGET}
    )

    target_compile_definitions(ot-ncp-ftd
        PRIVATE
            "DLPS_EN=0"
    )
endif()

##################################################
# ot-cli-mtd
##################################################
if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-cli-mtd")

    if(NOT DEFINED BLE_PERIPHERAL)
    set(BLE_PERIPHERAL "OFF")
    endif()

    if(${BLE_PERIPHERAL} STREQUAL "ON")
        target_sources(ot-cli-mtd
            PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/main.c
            ${REALTEK_SDK_ROOT}/bsp/power/io_dlps.c
            ${REALTEK_SDK_ROOT}/subsys/bluetooth/gatt_profile/src/server/bas.c
            ${OT_REALTEK_ROOT}/src/bee4/simple_ble_service_nus.c
            ${REALTEK_SDK_ROOT}/subsys/bluetooth/gatt_profile/src/client/ancs_client.c
            ${REALTEK_SDK_ROOT}/samples/bluetooth/ble_peripheral/src/ancs.c
            ${REALTEK_SDK_ROOT}/samples/bluetooth/ble_peripheral/src/app_task.c
            ${OT_REALTEK_ROOT}/src/bee4/peripheral_app.c
        )

        target_include_directories(ot-cli-mtd
            PRIVATE
                ${OT_REALTEK_ROOT}/src/bee4/${BUILD_TARGET}
                ${REALTEK_SDK_ROOT}/samples/bluetooth/ble_peripheral/src
                ${REALTEK_SDK_ROOT}/subsys/bluetooth/gatt_profile/inc/server
                ${REALTEK_SDK_ROOT}/subsys/bluetooth/gatt_profile/inc/client
                ${REALTEK_SDK_ROOT}/subsys/bluetooth/bt_host/inc
                ${REALTEK_SDK_ROOT}/bin/rtl87x2g/bt_host_image/bt_host_0_0
        )

        target_compile_options(ot-cli-mtd
            PRIVATE
                -include ${REALTEK_SDK_ROOT}/samples/bluetooth/ble_peripheral/src/app_flags.h
        )

        target_compile_definitions(openthread-bee4 PRIVATE "BUILD_BLE_PERIPHERAL")
    else()
        target_sources(ot-cli-mtd
            PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/main_ns.c
            ${REALTEK_SDK_ROOT}/bsp/power/io_dlps.c
        )

        target_include_directories(ot-cli-mtd
            PRIVATE
                ${OT_REALTEK_ROOT}/src/bee4/${BUILD_TARGET}
        )
    endif()

    target_compile_definitions(ot-cli-mtd
        PRIVATE
            "DLPS_EN=1"
    )

    target_compile_definitions(openthread-bee4
        PRIVATE
            "DLPS_EN=1"
    )
endif()

##################################################
# ot-rcp
##################################################
if(${OT_CMAKE_NINJA_TARGET} STREQUAL "ot-rcp" OR matter_enable_cfu)

    if(${BUILD_TYPE} STREQUAL "dev")
        target_compile_definitions(bee4-internal PUBLIC "BUILD_RCP=1")

        
        add_library(rtk_sign
            STATIC
            ./internal/rtk_sign/rtk_sign.c
        )


        target_include_directories(rtk_sign
            PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/internal/rtk_sign
            ${REALTEK_SDK_INCPATH}
            ${REALTEK_SDK_ROOT}/subsys/osif/inc
            ${REALTEK_SDK_ROOT}/subsys/mbedtls/repo/include
        )

        target_link_libraries(openthread-bee4
            PRIVATE
                rtk_sign
                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmbedcrypto.a
        )

        add_custom_command(
            TARGET openthread-bee4
            POST_BUILD
            COMMAND cp -f ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/librtk_sign.a ${PROJECT_SOURCE_DIR}/lib/bee4/
        )
    else()
        target_link_libraries(openthread-bee4
            PRIVATE
                ${PROJECT_SOURCE_DIR}/lib/bee4/librtk_sign.a
                ${PROJECT_SOURCE_DIR}/lib/bee4/libmbedcrypto.a
        )
    endif()

    target_include_directories(openthread-bee4
        PRIVATE
            ${REALTEK_SDK_ROOT}/subsys/cfu
            ${REALTEK_SDK_ROOT}/subsys/dfu
            ${OT_REALTEK_ROOT}/src/bee4/internal/cfu
            ${OT_REALTEK_ROOT}/src/bee4/internal/config_param
    )

    target_sources(openthread-bee4
        PRIVATE
        ${REALTEK_SDK_ROOT}/subsys/cfu/cfu.c
        ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/cfu_application.c
        ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/cfu_task.c
        ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/cfu_callback.c
        "${REALTEK_SDK_ROOT}/subsys/dfu/dfu_common.c"
        ${OT_REALTEK_ROOT}/src/bee4/main_ns.c
    )

    if(${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd")
        target_sources(openthread-bee4
            PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/usb_cfu_handle.c
        )
    elseif(${BUILD_BOARD_TARGET} STREQUAL "rtl8771guv" OR ${BUILD_BOARD_TARGET} STREQUAL "rtl8777g")
        target_sources(openthread-bee4
            PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/usb_cfu_handle.c
        )
    elseif(${BUILD_BOARD_TARGET} STREQUAL "rtl8771gtv")
        target_sources(openthread-bee4
            PRIVATE
            ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/uart_cfu_handle.c
            ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/loop_queue.c
            ${OT_REALTEK_ROOT}/src/bee4/internal/cfu/uart_transport.c
        )
    endif()

    target_compile_definitions(openthread-bee4
        PRIVATE
            "DLPS_EN=0"
    )

    target_link_options(openthread-bee4
        PUBLIC
            -Wl,--wrap,otInstanceResetRadioStack
    )

endif()

if(NOT (${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-ftd" OR ${OT_CMAKE_NINJA_TARGET} STREQUAL "matter-cli-mtd"))
    target_link_options(openthread-bee4
        PUBLIC
            -Wl,--wrap,otTaskletsSignalPending
    )

    if(${ENABLE_CLI} STREQUAL "ON")
        target_compile_definitions(openthread-bee4 PUBLIC "ENABLE_CLI=1")
    else()
        if(${BUILD_BOARD_TARGET} STREQUAL "rtl8777g_cert")
            target_compile_definitions(openthread-bee4 PUBLIC "ENABLE_CLI=1")
        else()
            target_compile_definitions(openthread-bee4 PUBLIC "ENABLE_CLI=0")
        endif()
    endif()
endif()
