
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

