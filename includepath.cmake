list (
    APPEND inc_path
        # sdk
        "${REALTEK_SDK_INCPATH}"
        # porting
        "${PROJECT_SOURCE_DIR}/src/${RT_PLATFORM}"
        "${PROJECT_SOURCE_DIR}/src/${RT_PLATFORM}/${BUILD_TARGET}"
        "${REALTEK_SDK_ROOT}/subsys/matter/vendor"
        "${REALTEK_SDK_ROOT}/subsys/matter/vendor/matter_ble"
        # lwip
        "${bee_lwip_path}/arch"
        "${bee_lwip_path}"
        "${bee_lwip_path}/../repo/include"
        "${bee_lwip_path}/../repo/include/lwip"
        "${bee_lwip_path}/../repo/include/lwip/apps"
        "${bee_lwip_path}/../repo/include/lwip/priv"
        "${bee_lwip_path}/../repo/include/lwip/prot"
        "${bee_lwip_path}/../repo/include/netif"
        "${bee_lwip_path}/../repo/include/netif/ppp"
        # openthread
        "${openthread_root}/src"
        "${openthread_root}/src/core"
        "${openthread_root}/include"
        "${openthread_root}/examples"
        "${openthread_root}/examples/platforms"
        "${openthread_root}/examples/platforms/utils"
        "${openthread_root}/cli"
        "${REALTEK_SDK_ROOT}/subsys/mbedtls"
        "${REALTEK_SDK_ROOT}/subsys/mbedtls/repo/include"
        "${REALTEK_SDK_ROOT}/subsys/mbedtls/repo/library"
)
