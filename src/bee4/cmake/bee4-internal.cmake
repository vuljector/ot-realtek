if(${BUILD_TYPE} STREQUAL "dev")
    add_library(bee4-internal
        # mac driver
        ${REALTEK_SDK_ROOT}/subsys/mac_driver/private/mac_driver_ext.c
        ${REALTEK_SDK_ROOT}/subsys/mac_driver/private/bee4/patch.c
        ${REALTEK_SDK_ROOT}/subsys/mac_driver/private/bee4/pta.c
        ${REALTEK_SDK_ROOT}/subsys/mac_driver/private/bee4/zb_pta_pin_mux.c
        ${REALTEK_SDK_ROOT}/subsys/mac_driver/private/zb_pta.c
        ${REALTEK_SDK_ROOT}/subsys/mac_driver/private/zb_sw_pta.c
        ${OT_REALTEK_ROOT}/src/bee4/internal/config_param/config_param_handle.c
    )

    target_include_directories(bee4-internal
        PRIVATE
            ${REALTEK_SDK_INCPATH}
            ${REALTEK_SDK_ROOT}/bsp/sdk_lib/inc
            ${REALTEK_SDK_ROOT}/subsys/mac_driver
            ${REALTEK_SDK_ROOT}/subsys/mac_driver/portable/bee4
            ${REALTEK_SDK_ROOT}/subsys/mac_driver/private
            ${REALTEK_SDK_ROOT}/subsys/mac_driver/private/bee4
            ${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_TARGET}
            ${CMAKE_CURRENT_SOURCE_DIR}/common
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REALTEK_SDK_ROOT}/../ROMExport/rtl87x2g/inc
            ${REALTEK_SDK_ROOT}/../ROMExport/rtl87x2g/inc/nsc
    )
endif()