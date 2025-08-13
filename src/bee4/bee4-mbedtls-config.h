/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BEE4_MBEDTLS_CONFIG_H
#define BEE4_MBEDTLS_CONFIG_H

#define MBEDTLS_CIPHER_MODE_CTR

#define MBEDTLS_ERROR_C
#define MBEDTLS_ERROR_STRERROR_DUMMY
#define MBEDTLS_HKDF_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_X509_CSR_PARSE_C
#define MBEDTLS_X509_CSR_WRITE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_CTR_DRBG_C

#ifdef MBEDTLS_MPI_MAX_SIZE
#undef MBEDTLS_MPI_MAX_SIZE
#define MBEDTLS_MPI_MAX_SIZE              1024 /**< Maximum number of bytes for usable MPIs. */
#endif

#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_ALT

#include "crypto_engine_nsc.h"

#define ENABLE_HW_RSA_VERIFY    (1)
#define ENABLE_HW_ECC_VERIFY    (1)
#define ENABLE_HW_EDDSA_VERIFY  (1)
#define ENABLE_HW_AES_VERIFY    (1)
#define ENABLE_HW_TRNG          (1)
#define ENABLE_HW_SHA256_VERIFY (1)

#if ENABLE_HW_ECC_VERIFY == 1 || ENABLE_HW_EDDSA_VERIFY == 1 || ENABLE_HW_RSA_VERIFY == 1
#define RTK_PUBKEY_ECC_MUL_ENTRY            0x1
#define RTK_PUBKEY_MOD_MUL_ENTRY            0x2
#define RTK_PUBKEY_MOD_ADD_ENTRY            0x3
#define RTK_PUBKEY_Px_MOD_ENTRY             0x4
#define RTK_PUBKEY_R_SQAR_ENTRY             0x5
#define RTK_PUBKEY_N_INV_ENTRY              0x6
#define RTK_PUBKEY_K_INV_ENTRY              0x7
#define RTK_PUBKEY_ECC_ADD_POINT_ENTRY      0x8
#define RTK_PUBKEY_ECC_POINT_CHECK_ENTRY    0x9
#define RTK_PUBKEY_SET_A_FROM_P_ENTRY       0xa
#define RTK_PUBKEY_K_INV_BIN_ENTRY          0x11
#define RTK_PUBKEY_MOD_MUL_BIN_ENTRY        0xb
#define RTK_PUBKEY_MOD_ADD_BIN_ENTRY        0xc
#define RTK_PUBKEY_MOD_COMP_BIN_ENTRY       0xd
#define RTK_PUBKEY_X_MOD_N_ENTRY            0xe
#define RTK_PUBKEY_MOD_SUB_ENTRY            0xf

#define GO_TO_END_LOOP                      1
#define RR_MOD_N_READY                      1

#define ECC_PRIME_MODE                      0   // 3'b000
#define ECC_BINARY_MODE                     1   // 3'b001
#define RSA_MODE                            2   // 3'b010
#define ECC_EDWARDS_CURVE                   3   // 3'b011
#define ECC_MONTGOMERY_CURVE                7   // 3'b111

#define PKE_MMEM_ADDR                       0x50090000

#define RSA_N_ADDR                          (PKE_MMEM_ADDR + 0)
#define RSA_E_ADDR                          (PKE_MMEM_ADDR + 0x180)
#define RSA_A_ADDR                          (PKE_MMEM_ADDR + 0x300)
#define RSA_RESULT_ADDR                     (PKE_MMEM_ADDR + 0x480)
#define RSA_GEN_KEY_RESULT_ADDR             (PKE_MMEM_ADDR + 0x3CC)

#define ECC_N_ADDR                          (PKE_MMEM_ADDR + 0)
#define ECC_E_ADDR                          (PKE_MMEM_ADDR + 0x40)
#define ECC_CURVE_A_ADDR                    (PKE_MMEM_ADDR + 0x80)
#define ECC_RR_MOD_N_ADDR                   (PKE_MMEM_ADDR + 0xC0)
#define ECC_CURVE_B_ADDR                    (PKE_MMEM_ADDR + 0x100)
#define ECC_X_ADDR                          (PKE_MMEM_ADDR + 0x140)
#define ECC_Y_ADDR                          (PKE_MMEM_ADDR + 0x180)
#define ECC_Z_ADDR                          (PKE_MMEM_ADDR + 0x1C0)
#define ECC_X_RESULT_ADDR                   (PKE_MMEM_ADDR + 0x200)
#define ECC_Y_RESULT_ADDR                   (PKE_MMEM_ADDR + 0x240)
#define ECC_Z_RESULT_ADDR                   (PKE_MMEM_ADDR + 0x280)
#endif

#endif // BEE4_MBEDTLS_CONFIG_H
