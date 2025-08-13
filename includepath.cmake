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
list (
    APPEND inc_path
        # sdk
        "${REALTEK_SDK_INCPATH}"
        # porting
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
