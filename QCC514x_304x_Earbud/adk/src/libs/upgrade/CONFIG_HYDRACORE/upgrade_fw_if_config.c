/****************************************************************************
Copyright (c) 2016 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_fw_if_config.c

DESCRIPTION
    Configuration dependent functions which (largely) interact with the firmware.

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <stdlib.h>
#include <string.h>
#include <logging.h>
#include <sink.h>
#include <stream.h>
#include <panic.h>
#include <byte_utils.h>
#include <hydra_macros.h>
#include <rsa_pss_constants.h>
#include <rsa_decrypt.h>
#include <imageupgrade.h>

#include "upgrade_ctx.h"
#include "upgrade_fw_if.h"
#include "secrsa_padding.h"

uint8 UpgradePartitionDataGetSigningMode(void);

/* The number of uint8s in a SHA-256 signature */
#define SHA_256_HASH_LENGTH (256/8)

/*
 * The expected signing mode types:
 * "standard" for ADK6.1 with all partitions signed;
 * "compatibility" for ADK6.0 with just the image header signed.
 */
typedef enum
{
    IMAGE_HEADER_SIGNING_MODE       = 0,
    ALL_PARTITIONS_SIGNING_MODE     = 1
} SIGNING_MODE_t;

/* A bit map of up to 32 partitions that are being processed in this DFU file. */
static uint32 partitionMap;

/******************************************************************************
NAME
    UpgradeFWIFAudioDFUExists

DESCRIPTION
    Checks if the audio dfu bit is set in the partition map.

RETURNS
    bool TRUE if the bit audio dfu is set.
*/
bool UpgradeFWIFAudioDFUExists(void)
{
    if ((partitionMap & (1 << IMAGE_SECTION_AUDIO_IMAGE)) != 0)
    {
        return TRUE;
    }
    
    return FALSE;
}

/******************************************************************************
NAME
    UpgradeFWIFGetHeaderID

DESCRIPTION
    Get the identifier for the header of an upgrade file.

RETURNS
    const char * Pointer to the header string.
*/
const char *UpgradeFWIFGetHeaderID(void)
{
    return "APPUHDR5";
}


/***************************************************************************
NAME
    UpgradeFWIFPartitionWrite

DESCRIPTION
    Write data to an open external flash partition. Each byte of the data
    is copied to the partition in a byte by byte copy operation.

PARAMS
    handle Handle to a writeable partition.
    data Pointer to the data to write.
    len Number of bytes (not words) to write.

RETURNS
    uint16 The number of bytes written, or 0 if there was an error.
*/
uint16 UpgradeFWIFPartitionWrite(UpgradeFWIFPartitionHdl handle, const uint8 *data, uint16 data_len)
{
    Sink sink = (Sink)(int)handle;
    if (!sink)
        return 0;

    uint16 data_remaining = data_len;
    while (data_remaining)
    {
        uint8 *dest = SinkMap(sink);
        if (dest)
        {
            const uint16 write_len = MIN(SinkSlack(sink), data_remaining);
            if (SinkClaim(sink, write_len) != 0xFFFF)
            {
                DEBUG_LOG_VERBOSE("UpgradeFWIFPartitionWrite, claimed %u bytes for writing", write_len);

                memmove(dest, data, write_len);
                if (SinkFlushBlocking(sink, write_len))
                {
                    data += write_len;
                    data_remaining -= write_len;
                }
                else
                {
                    DEBUG_LOG_ERROR("UpgradeFWIFPartitionWrite, failed to flush data to partition, length %u", write_len);
                    break;
                }
            }
            else
            {
                DEBUG_LOG_ERROR("UpgradeFWIFPartitionWrite, failed to claim %u bytes for writing", write_len);
                break;
            }

        }
        else
        {
            DEBUG_LOG_ERROR("UpgradeFWIFPartitionWrite, failed to map sink %p", (void *)sink);
            break;
        }
    }

    return data_len - data_remaining;
}

/***************************************************************************
NAME
    UpgradeFWIFValidateInit

DESCRIPTION
    Initialise the signature validation context.
    As we're not updating partition data to the context, the context is only
    created when UpgradeFWIFValidateFinalize() is invoked to validate 
    the signature.

RETURNS
*/
void UpgradeFWIFValidateInit(void)
{
    DEBUG_LOG_INFO("UpgradeFWIFValidateInit");
    partitionMap = 0;
}

/***************************************************************************
NAME
    UpgradeFWIFValidateUpdate

DESCRIPTION
    Update the validation context with the next set of data.

PARAMS
    buffer Pointer to the next set of data (unused)
    partNum The partition number

RETURNS
    bool TRUE if a validation context is updated successfully, FALSE otherwise.
*/
bool UpgradeFWIFValidateUpdate(uint8 *buffer, uint16 partNum)
{
    /* This function is not necessary for CONFIG_HYDRACORE. */
    UNUSED(buffer);
    DEBUG_LOG_INFO("UpgradeFWIFValidateUpdate, partition %u", partNum);
    if (partNum < IMAGE_SECTION_ID_MAX)
    {
        partitionMap |= 1 << partNum;
        return TRUE;
    }
    DEBUG_LOG_ERROR("UpgradeFWIFValidateUpdate, invalid partition %u", partNum);
    return FALSE;
}

/***************************************************************************
NAME
    UpgradeFWIFValidateStart

DESCRIPTION
    Verify the accumulated data in the validation context against
    the given signature - initial request.

PARAMS
    P0 Hash context.

RETURNS
    UpgradeHostErrorCode Status code.
*/
UpgradeHostErrorCode UpgradeFWIFValidateStart(hash_context_t *vctx)
{
    uint8 SigningMode = UpgradePartitionDataGetSigningMode();
    
    UpgradeHostErrorCode errorCode;
    
    switch (SigningMode)
    {
        case IMAGE_HEADER_SIGNING_MODE:
            if (ImageUpgradeHashSectionUpdate(vctx, IMAGE_SECTION_APPS_P0_HEADER))
            {
                DEBUG_LOG_INFO("UpgradeFWIFValidateStart, ImageUpgradeHashSectionUpdate successful");
                errorCode = UPGRADE_HOST_OEM_VALIDATION_SUCCESS;
            }
            else
            {
                DEBUG_LOG_INFO("UpgradeFWIFValidateStart, ImageUpgradeHashSectionUpdate failed");
                ImageUpgradeHashFinalise(vctx, NULL, SHA_256_HASH_LENGTH);
                errorCode = UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_FOOTER;
            }
            break;

        case ALL_PARTITIONS_SIGNING_MODE:    
            /*
             * We have a valid context, so update it with the image sections that
             * are indicated by the set bits in the partitionMap.
             */
            DEBUG_LOG_INFO("UpgradeFWIFValidateStart, ImageUpgradeHashAllSectionsUpdate");
            ImageUpgradeHashAllSectionsUpdate(vctx);
            errorCode = UPGRADE_HOST_HASHING_IN_PROGRESS;
            break;

        default:
            DEBUG_LOG_INFO("UpgradeFWIFValidateStart, unknown signing mode %u", SigningMode);
            ImageUpgradeHashFinalise(vctx, NULL, SHA_256_HASH_LENGTH);
            errorCode = UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_FOOTER;
    }
    
    return errorCode;
}

/***************************************************************************
NAME
    UpgradeFWIFValidateFinish

DESCRIPTION
    Verify the accumulated data in the validation context against
    the given signature - finish request.

PARAMS
    signature Pointer to the signature to compare against.
    The signature contains the RSA-2048 encrypted, PSS padded, SHA-256 hash.
    The RSA-2048 encrypted signature is 128 bytes.
    (RSA-1024 can be used instead of RSA-2148 by defining the UPGRADE_RSA_1024
    conditional compilation flag. The RSA-1024 encrypted signature is 64 bytes.)
    After decryption the PSS padded signature is also 128 bytes.
    After decoding, the SHA-256 signature hash is 32 bytes.
    This is to be compared against the calculated SHA-256 hash that is
    calculated from the IMAGE_SECTION_APPS_P0_HEADER in the SQIF.

RETURNS
    bool TRUE if validation is successful, FALSE otherwise.
*/
bool UpgradeFWIFValidateFinish(hash_context_t *vctx, uint8 *signature)
{
    uint16 workSignature[RSA_SIGNATURE_SIZE];
    uint16 reworkSignature[RSA_SIGNATURE_SIZE];
    uint8 sectionHash[SHA_256_HASH_LENGTH];
    uint16 sign_r2n[RSA_SIGNATURE_SIZE];
    int verify_result;

    /* sanity check to make sure the use of defines for array sizes in the function 
     * matches the original use of sizeof(). sizeof() removed in case malloc used.
     */
    COMPILE_TIME_ASSERT(sizeof(workSignature) == (sizeof(uint16) * RSA_SIGNATURE_SIZE),
            compiler_assumptions_changed_u16);
    COMPILE_TIME_ASSERT(sizeof(sectionHash) == SHA_256_HASH_LENGTH,
            compiler_assumptions_changed_u8);

    /* Get the result of the ImageUpgradeHashSectionUpdate */
    if (!ImageUpgradeHashFinalise(vctx, sectionHash, SHA_256_HASH_LENGTH))
    {
        DEBUG_LOG_ERROR("UpgradeFWIFValidateFinish, ImageUpgradeHashSectionUpdate failed");
        return FALSE;
    }

    /*
     * Copy the encrypted padded signature given into the workSignature array
     * as the rsa_decrypt will modify the workSignature array to contain the
     * output decrypted PSS padded SHA-256 signature, and we don't want to
     * trample on the original input supplied.
     */
    ByteUtilsMemCpy16((uint8 *)workSignature, 0, (const uint16 *) signature, 0,
                        RSA_SIGNATURE_SIZE * sizeof(uint16));

    /*
     * Copy the constant rsa_decrypt_constant_sign_r2n array into a writable
     * array as it will get modified in the rsa_decrypt process.
     */
    memcpy(sign_r2n, rsa_decrypt_constant_sign_r2n, 
                        RSA_SIGNATURE_SIZE * sizeof(uint16));

    /*
     * Decrypt the RSA-2048 encrypted PSS padded signature.
     * The result is the PSS padded signature returned in the workSignature
     * array that is also used fopr the input.
     */
    rsa_decrypt(workSignature, &rsa_decrypt_constant_mod, sign_r2n);

    /*
     * The ce_pkcs1_pss_padding_verify was failing on looking for 0xbc in the
     * last byte of the workSignature output by rsa_decrypt, and it was in the
     * penultimate byte, so swap the uint16 endian-ness from workSignature into
     * reworkSignature and supply that as input to ce_pkcs1_pss_padding_verify
     * instead.
     */
    ByteUtilsMemCpy16((uint8 *)reworkSignature, 0, (const uint16 *) workSignature, 0,
                    RSA_SIGNATURE_SIZE * sizeof(uint16));
    /*
     * Verify the PSS padded signature in reworkSignature against the
     * SHA-256 hash in from the image section in the sectionHash.
     */
    verify_result = ce_pkcs1_pss_padding_verify(
                                (const unsigned char *) sectionHash,
                                SHA_256_HASH_LENGTH,
                                (const unsigned char *) reworkSignature,
                                RSA_SIGNATURE_SIZE * sizeof(uint16),
                                ce_pkcs1_pss_padding_verify_constant_saltlen,
                                sizeof(rsa_decrypt_constant_mod.M) * 8);

    DEBUG_LOG_ERROR("UpgradeFWIFValidateFinish, ce_pkcs1_pss_padding_verify result %u", verify_result);
    if (verify_result != CE_SUCCESS)
    {
        DEBUG_LOG_ERROR("UpgradeFWIFValidateFinish, failed");
        return FALSE;
    }

    DEBUG_LOG_INFO("UpgradeFWIFValidateFinish, passed");
    return TRUE;
}

bool UpgradeFWIFValidateAddPartitionData(uint16 partition, uint32 skip, uint32 *read)
{
    /* This function is not necessary for CONFIG_HYDRACORE. */
    UNUSED(partition);
    UNUSED(skip);
    UNUSED(read);
    return TRUE;
}

UpgradeFWIFPartitionValidationStatus 
UpgradeFWIFValidateExecutablePartition(uint16 physPartition)
{
    UNUSED(physPartition);
    return UPGRADE_FW_IF_PARTITION_VALIDATION_SKIP;
}

UpgradeFWIFApplicationValidationStatus UpgradeFWIFValidateApplication(void)
{
    return UPGRADE_FW_IF_APPLICATION_VALIDATION_SKIP;
}
