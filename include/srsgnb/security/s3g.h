/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

/*
 * Adapted from ETSI/SAGE specifications:
 * "Specification of the 3GPP Confidentiality and
 * Integrity Algorithms UEA2 & UIA2.
 * Document 1: UEA2 and UIA2 Specification"
 * "Specification of the 3GPP Confidentiality
 * and Integrity Algorithms UEA2 & UIA2.
 * Document 2: SNOW 3G Specification"
 */

#pragma once

#include "security.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace srsgnb {
namespace security {

typedef struct {
  uint32_t* lfsr;
  uint32_t* fsm;
} S3G_STATE;

/* Initialization.
 * Input k[4]: Four 32-bit words making up 128-bit key.
 * Input IV[4]: Four 32-bit words making 128-bit initialization variable.
 * Output: All the LFSRs and FSM are initialized for key generation.
 * See Section 4.1.
 */

void s3g_initialize(S3G_STATE* state, uint32_t k[4], uint32_t iv[4]);

/*********************************************************************
    Name: s3g_deinitialize

    Description: Deinitialization.

    Document Reference: Specification of the 3GPP Confidentiality and
                            Integrity Algorithms UEA2 & UIA2 D2 v1.1
*********************************************************************/
void s3g_deinitialize(S3G_STATE* state);

/* Generation of Keystream.
 * input n: number of 32-bit words of keystream.
 * input z: space for the generated keystream, assumes
 * memory is allocated already.
 * output: generated keystream which is filled in z
 * See section 4.2.
 */

void s3g_generate_keystream(S3G_STATE* state, uint32_t n, uint32_t* ks);

/* f8.
 * Input key: 128 bit Confidentiality Key.
 * Input count:32-bit Count, Frame dependent input.
 * Input bearer: 5-bit Bearer identity (in the LSB side).
 * Input dir:1 bit, direction of transmission.
 * Input data: length number of bits, input bit stream.
 * Input length: 32 bit Length, i.e., the number of bits to be encrypted or
 * decrypted.
 * Output data: Output bit stream. Assumes data is suitably memory
 * allocated.
 * Encrypts/decrypts blocks of data between 1 and 2^32 bits in length as
 * defined in Section 3.
 */

// void snow3g_f8(u8* key, u32 count, u32 bearer, u32 dir, u8* data, u32 length);

/* f9.
 * Output : 32 bit block used as MAC
 * Input key: 128 bit Integrity Key.
 * Input count:32-bit Count, Frame dependent input.
 * Input fresh: 32-bit Random number.
 * Input dir:1 bit, direction of transmission (in the LSB).
 * Input data: length number of bits, input bit stream.
 * Input length: 64 bit Length, i.e., the number of bits to be MAC'd.
 * Generates 32-bit MAC using UIA2 algorithm as defined in Section 4.
 */

void s3g_f9(sec_mac&       mac,
            const uint8_t* key,
            uint32_t       count,
            uint32_t       fresh,
            uint32_t       dir,
            uint8_t*       data,
            uint64_t       length);

} // namespace security
} // namespace srsgnb
