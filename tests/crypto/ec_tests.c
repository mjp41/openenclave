// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#if defined(OE_BUILD_ENCLAVE)
#include <openenclave/enclave.h>
#endif

#include <openenclave/bits/safecrt.h>
#include <openenclave/internal/asn1.h>
#include <openenclave/internal/cert.h>
#include <openenclave/internal/ec.h>
#include <openenclave/internal/hexdump.h>
#include <openenclave/internal/raise.h>
#include <openenclave/internal/random.h>
#include <openenclave/internal/tests.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "readfile.h"
#include "tests.h"
#include "utils.h"

/* _CERT use as a ec_cert_with_ext.pem
 * _SGX_CERT use as a ec_cert_crl_distribution.pem
 * _CERT_WITHOUT_EXTENSIONS use as a Leafec.crt.pem
 * _CHAIN use as a Certificate chain organized from leaf-to-root
 * _PRIVATE_KEY use as a Rootec.key.pem
 * _PUBLIC_KEY use as a Rootec.public.key
 * _SIGNATURE use as a test_ec_signature
 * private_key_pem use as a Rootec.key.pem
 * public_key_pem use as a Rootec.public.key
 */

/* Certificate with an EC key */
static char _CERT[max_cert_size];
static char _SGX_CERT[max_cert_size];

/* A certificate without any extensions */
static char _CERT_WITHOUT_EXTENSIONS[max_cert_size];

/* Certificate chain organized from leaf-to-root */
static char _CHAIN[max_cert_chains_size];
static char _PRIVATE_KEY[max_key_size];
static char _PUBLIC_KEY[max_key_size];
static uint8_t _SIGNATURE[max_sign_size];
uint8_t private_key_pem[max_key_size];
uint8_t public_key_pem[max_key_size];
uint8_t x_data[max_coordinates_size];
uint8_t y_data[max_coordinates_size];

size_t private_key_size;
size_t public_key_size;
size_t sign_size;
size_t x_size, y_size;

static const uint8_t _P256_GROUP_ORDER[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17,
    0x9E, 0x84, 0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51,
};

// Test EC signing operation over an ASCII alphabet string. Note that two
// signatures over the same data produce different hex sequences, although
// signature verification will still succeed.
static void _test_sign_and_verify()
{
    printf("=== begin %s()\n", __FUNCTION__);

    uint8_t* signature = NULL;
    size_t signature_size = 0;
    oe_result_t r;

    {
        oe_ec_private_key_t key = {0};

        r = oe_ec_private_key_read_pem(
            &key, (const uint8_t*)_PRIVATE_KEY, strlen(_PRIVATE_KEY) + 1);
        OE_TEST(r == OE_OK);

        r = oe_ec_private_key_sign(
            &key,
            OE_HASH_TYPE_SHA256,
            &ALPHABET_HASH,
            sizeof(ALPHABET_HASH),
            signature,
            &signature_size);
        OE_TEST(r == OE_BUFFER_TOO_SMALL);

        OE_TEST(signature = (uint8_t*)malloc(signature_size));

        r = oe_ec_private_key_sign(
            &key,
            OE_HASH_TYPE_SHA256,
            &ALPHABET_HASH,
            sizeof(ALPHABET_HASH),
            signature,
            &signature_size);
        OE_TEST(r == OE_OK);

        OE_TEST(signature != NULL);
        OE_TEST(signature_size != 0);

        oe_ec_private_key_free(&key);
    }

    {
        oe_ec_public_key_t key = {0};

        r = oe_ec_public_key_read_pem(
            &key, (const uint8_t*)_PUBLIC_KEY, strlen(_PUBLIC_KEY) + 1);
        OE_TEST(r == OE_OK);

        r = oe_ec_public_key_verify(
            &key,
            OE_HASH_TYPE_SHA256,
            &ALPHABET_HASH,
            sizeof(ALPHABET_HASH),
            signature,
            signature_size);
        OE_TEST(r == OE_OK);

        r = oe_ec_public_key_verify(
            &key,
            OE_HASH_TYPE_SHA256,
            &ALPHABET_HASH,
            sizeof(ALPHABET_HASH),
            _SIGNATURE,
            sign_size);
        OE_TEST(r == OE_OK);

        oe_ec_public_key_free(&key);
    }

    /* Convert a known signature to raw form and back */
    {
        const uint8_t SIG[] = {
            0x30, 0x45, 0x02, 0x20, 0x6A, 0xCD, 0x74, 0xB9, 0x8B, 0x1A, 0xDD,
            0xA3, 0x3D, 0x84, 0x42, 0x44, 0x1F, 0x9B, 0x62, 0x5E, 0x9E, 0xB7,
            0x3F, 0x3C, 0x89, 0xFD, 0xFA, 0xFE, 0x2B, 0x25, 0x7C, 0x43, 0x29,
            0xE3, 0x3D, 0x43, 0x02, 0x21, 0x00, 0xDE, 0xEB, 0x54, 0xF8, 0x6C,
            0x7D, 0xCD, 0xA2, 0x0D, 0x8B, 0x10, 0xCB, 0x4D, 0x7D, 0x8B, 0x14,
            0xDC, 0x54, 0x83, 0x87, 0xD3, 0x35, 0x5A, 0x48, 0xD1, 0x67, 0xD1,
            0xF0, 0xA8, 0x4B, 0x31, 0xBE,
        };
        const uint8_t R[] = {
            0x6A, 0xCD, 0x74, 0xB9, 0x8B, 0x1A, 0xDD, 0xA3, 0x3D, 0x84, 0x42,
            0x44, 0x1F, 0x9B, 0x62, 0x5E, 0x9E, 0xB7, 0x3F, 0x3C, 0x89, 0xFD,
            0xFA, 0xFE, 0x2B, 0x25, 0x7C, 0x43, 0x29, 0xE3, 0x3D, 0x43,
        };

        const uint8_t S[] = {
            0xDE, 0xEB, 0x54, 0xF8, 0x6C, 0x7D, 0xCD, 0xA2, 0x0D, 0x8B, 0x10,
            0xCB, 0x4D, 0x7D, 0x8B, 0x14, 0xDC, 0x54, 0x83, 0x87, 0xD3, 0x35,
            0x5A, 0x48, 0xD1, 0x67, 0xD1, 0xF0, 0xA8, 0x4B, 0x31, 0xBE,

        };
        uint8_t data[sizeof(SIG)];
        size_t size = sizeof(data);
        r = oe_ecdsa_signature_write_der(
            data, &size, R, sizeof(R), S, sizeof(S));
        OE_TEST(r == OE_OK);
        OE_TEST(sizeof(SIG) == size);
        OE_TEST(memcmp(SIG, data, sizeof(SIG)) == 0);
    }

    free(signature);

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_generate_common(
    const oe_ec_private_key_t* private_key,
    const oe_ec_public_key_t* public_key)
{
    oe_result_t r;
    uint8_t* signature = NULL;
    size_t signature_size = 0;

    r = oe_ec_private_key_sign(
        private_key,
        OE_HASH_TYPE_SHA256,
        &ALPHABET_HASH,
        sizeof(ALPHABET_HASH),
        signature,
        &signature_size);
    OE_TEST(r == OE_BUFFER_TOO_SMALL);

    OE_TEST(signature = (uint8_t*)malloc(signature_size));

    r = oe_ec_private_key_sign(
        private_key,
        OE_HASH_TYPE_SHA256,
        &ALPHABET_HASH,
        sizeof(ALPHABET_HASH),
        signature,
        &signature_size);
    OE_TEST(r == OE_OK);

    r = oe_ec_public_key_verify(
        public_key,
        OE_HASH_TYPE_SHA256,
        &ALPHABET_HASH,
        sizeof(ALPHABET_HASH),
        signature,
        signature_size);
    OE_TEST(r == OE_OK);

    free(signature);
}

static void _test_generate()
{
    printf("=== begin %s()\n", __FUNCTION__);

    oe_result_t r;
    oe_ec_private_key_t private_key = {0};
    oe_ec_public_key_t public_key = {0};

    r = oe_ec_generate_key_pair(
        OE_EC_TYPE_SECP256R1, &private_key, &public_key);
    OE_TEST(r == OE_OK);

    _test_generate_common(&private_key, &public_key);

    oe_ec_private_key_free(&private_key);
    oe_ec_public_key_free(&public_key);

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_generate_from_private()
{
    printf("=== begin %s()\n", __FUNCTION__);

    oe_result_t r;
    uint8_t private_raw[32];
    oe_ec_private_key_t private_key = {0};
    oe_ec_public_key_t public_key = {0};
    oe_ec_private_key_t private_key2 = {0};
    oe_ec_public_key_t public_key2 = {0};
    bool equal = false;

    /* Generate a random 256 bit key. */
    r = oe_random_internal(private_raw, sizeof(private_raw));
    OE_TEST(r == OE_OK);

    /* Set the MSB to 0 so we always have a valid NIST 256P key. */
    private_raw[0] = private_raw[0] & 0x7F;

    r = oe_ec_generate_key_pair_from_private(
        OE_EC_TYPE_SECP256R1,
        private_raw,
        sizeof(private_raw),
        &private_key,
        &public_key);
    OE_TEST(r == OE_OK);

    /* Test that signing works with ECC key. */
    _test_generate_common(&private_key, &public_key);

    /* Test that the key generation is deterministic. */
    r = oe_ec_generate_key_pair_from_private(
        OE_EC_TYPE_SECP256R1,
        private_raw,
        sizeof(private_raw),
        &private_key2,
        &public_key2);
    OE_TEST(r == OE_OK);

    _test_generate_common(&private_key2, &public_key2);

    r = oe_ec_public_key_equal(&public_key, &public_key2, &equal);
    OE_TEST(r == OE_OK);
    OE_TEST(equal);

    oe_ec_private_key_free(&private_key);
    oe_ec_public_key_free(&public_key);
    oe_ec_private_key_free(&private_key2);
    oe_ec_public_key_free(&public_key2);

    /* Key limits are 1 <= key <= order. Test 0 key fails. */
    OE_TEST(
        oe_memset_s(private_raw, sizeof(private_raw), 0, sizeof(private_raw)) ==
        OE_OK);

    r = oe_ec_generate_key_pair_from_private(
        OE_EC_TYPE_SECP256R1,
        private_raw,
        sizeof(private_raw),
        &private_key,
        &public_key);
    OE_TEST(r != OE_OK);

    /* Test key = order fails. */
    OE_TEST(
        oe_memcpy_s(
            private_raw,
            sizeof(private_raw),
            _P256_GROUP_ORDER,
            sizeof(_P256_GROUP_ORDER)) == OE_OK);

    r = oe_ec_generate_key_pair_from_private(
        OE_EC_TYPE_SECP256R1,
        private_raw,
        sizeof(private_raw),
        &private_key,
        &public_key);
    OE_TEST(r != OE_OK);

    /* Test key = 1 passes. */
    OE_TEST(
        oe_memset_s(private_raw, sizeof(private_raw), 0, sizeof(private_raw)) ==
        OE_OK);

    private_raw[sizeof(private_raw) - 1] |= 0x1;

    r = oe_ec_generate_key_pair_from_private(
        OE_EC_TYPE_SECP256R1,
        private_raw,
        sizeof(private_raw),
        &private_key,
        &public_key);
    OE_TEST(r == OE_OK);

    _test_generate_common(&private_key, &public_key);
    oe_ec_private_key_free(&private_key);
    oe_ec_public_key_free(&public_key);

    /* Test key = order - 1 passes. */
    OE_TEST(
        oe_memcpy_s(
            private_raw,
            sizeof(private_raw),
            _P256_GROUP_ORDER,
            sizeof(_P256_GROUP_ORDER)) == OE_OK);

    private_raw[sizeof(private_raw) - 1] &= 0xFE;

    r = oe_ec_generate_key_pair_from_private(
        OE_EC_TYPE_SECP256R1,
        private_raw,
        sizeof(private_raw),
        &private_key,
        &public_key);
    OE_TEST(r == OE_OK);

    _test_generate_common(&private_key, &public_key);
    oe_ec_private_key_free(&private_key);
    oe_ec_public_key_free(&public_key);

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_private_key_limits()
{
    uint8_t key[32] = {0};
    bool valid;

    /* Limits are 1 <= key <= _P256_GROUP_ORDER. Try key = 0 first. */
    valid = oe_ec_valid_raw_private_key(OE_EC_TYPE_SECP256R1, key, sizeof(key));
    OE_TEST(!valid);

    /* Try key = 1. */
    key[sizeof(key) - 1] |= 0x01;
    valid = oe_ec_valid_raw_private_key(OE_EC_TYPE_SECP256R1, key, sizeof(key));
    OE_TEST(valid);

    /* Try _P256_GROUP_ORDER - 1. */
    OE_TEST(
        oe_memcpy_s(
            key, sizeof(key), _P256_GROUP_ORDER, sizeof(_P256_GROUP_ORDER)) ==
        OE_OK);

    key[sizeof(key) - 1] &= 0xFE;
    valid = oe_ec_valid_raw_private_key(OE_EC_TYPE_SECP256R1, key, sizeof(key));
    OE_TEST(valid);

    /* Try key = _P256_GROUP_ORDER. */
    key[sizeof(key) - 1] |= 0x01;
    valid = oe_ec_valid_raw_private_key(OE_EC_TYPE_SECP256R1, key, sizeof(key));
    OE_TEST(!valid);
}

static void _test_write_private()
{
    printf("=== begin %s()\n", __FUNCTION__);

    oe_result_t r;
    oe_ec_public_key_t public_key = {0};
    oe_ec_private_key_t key1 = {0};
    oe_ec_private_key_t key2 = {0};
    uint8_t* pem_data1 = NULL;
    size_t pem_size1 = 0;
    uint8_t* pem_data2 = NULL;
    size_t pem_size2 = 0;

    r = oe_ec_generate_key_pair(OE_EC_TYPE_SECP256R1, &key1, &public_key);
    OE_TEST(r == OE_OK);

    {
        r = oe_ec_private_key_write_pem(&key1, pem_data1, &pem_size1);
        OE_TEST(r == OE_BUFFER_TOO_SMALL);

        OE_TEST(pem_data1 = (uint8_t*)malloc(pem_size1));

        r = oe_ec_private_key_write_pem(&key1, pem_data1, &pem_size1);
        OE_TEST(r == OE_OK);
    }

    OE_TEST(pem_size1 != 0);
    OE_TEST(pem_data1[pem_size1 - 1] == '\0');
    OE_TEST(strlen((char*)pem_data1) == pem_size1 - 1);

    r = oe_ec_private_key_read_pem(&key2, pem_data1, pem_size1);
    OE_TEST(r == OE_OK);

    {
        r = oe_ec_private_key_write_pem(&key2, pem_data2, &pem_size2);
        OE_TEST(r == OE_BUFFER_TOO_SMALL);

        OE_TEST(pem_data2 = (uint8_t*)malloc(pem_size2));

        r = oe_ec_private_key_write_pem(&key2, pem_data2, &pem_size2);
        OE_TEST(r == OE_OK);
    }

    OE_TEST(pem_size1 == pem_size2);
    OE_TEST(memcmp(pem_data1, pem_data2, pem_size1) == 0);

    free(pem_data1);
    free(pem_data2);
    oe_ec_public_key_free(&public_key);
    oe_ec_private_key_free(&key1);
    oe_ec_private_key_free(&key2);

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_write_public()
{
    printf("=== begin %s()\n", __FUNCTION__);

    oe_result_t r;
    oe_ec_public_key_t key = {0};
    void* pem_data = NULL;
    size_t pem_size = 0;

    r = oe_ec_public_key_read_pem(
        &key, (const uint8_t*)_PUBLIC_KEY, strlen(_PUBLIC_KEY) + 1);
    OE_TEST(r == OE_OK);

    {
        r = oe_ec_public_key_write_pem(&key, pem_data, &pem_size);
        OE_TEST(r == OE_BUFFER_TOO_SMALL);

        OE_TEST(pem_data = (uint8_t*)malloc(pem_size));

        r = oe_ec_public_key_write_pem(&key, pem_data, &pem_size);
        OE_TEST(r == OE_OK);
    }

    OE_TEST((strlen(_PUBLIC_KEY) + 1) == pem_size);
    OE_TEST(memcmp(_PUBLIC_KEY, pem_data, pem_size) == 0);

    free(pem_data);
    oe_ec_public_key_free(&key);

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_cert_methods()
{
    printf("=== begin %s()\n", __FUNCTION__);

    oe_result_t r;

    /* Test oe_cert_get_ec_public_key() */
    {
        oe_cert_t cert = {0};
        oe_ec_public_key_t key = {0};

        r = oe_cert_read_pem(&cert, _CERT, strlen(_CERT) + 1);
        OE_TEST(r == OE_OK);

        r = oe_cert_get_ec_public_key(&cert, &key);
        OE_TEST(r == OE_OK);

        /* Test oe_ec_public_key_equal() */
        {
            bool equal;
            OE_TEST(oe_ec_public_key_equal(&key, &key, &equal) == OE_OK);
            OE_TEST(equal == true);
        }

        oe_ec_public_key_free(&key);
        oe_cert_free(&cert);
    }

    /* Test oe_cert_chain_get_cert() */
    {
        oe_cert_chain_t chain;

        /* Load the chain from PEM format */
        r = oe_cert_chain_read_pem(&chain, _CHAIN, strlen(_CHAIN) + 1);
        OE_TEST(r == OE_OK);

        /* Get the length of the chain */
        size_t length;
        r = oe_cert_chain_get_length(&chain, &length);
        OE_TEST(r == OE_OK);
        OE_TEST(length == 3);

        /* Get each certificate in the chain */
        for (size_t i = 0; i < length; i++)
        {
            oe_cert_t cert;
            r = oe_cert_chain_get_cert(&chain, i, &cert);
            OE_TEST(r == OE_OK);
            oe_cert_free(&cert);
        }

        /* Test out of bounds */
        {
            oe_cert_t cert;
            r = oe_cert_chain_get_cert(&chain, length + 1, &cert);
            OE_TEST(r == OE_OUT_OF_BOUNDS);
            oe_cert_free(&cert);
        }

        oe_cert_chain_free(&chain);
    }

    /* Test oe_cert_chain_get_root_cert() and oe_cert_chain_get_leaf_cert() */
    {
        oe_cert_chain_t chain;
        oe_cert_t root;
        oe_cert_t leaf;

        /* Load the chain from PEM format */
        r = oe_cert_chain_read_pem(&chain, _CHAIN, strlen(_CHAIN) + 1);
        OE_TEST(r == OE_OK);

        /* Get the root certificate */
        r = oe_cert_chain_get_root_cert(&chain, &root);
        OE_TEST(r == OE_OK);

        /* Get the leaf certificate */
        r = oe_cert_chain_get_leaf_cert(&chain, &leaf);
        OE_TEST(r == OE_OK);

        /* Check that the keys are identical for top and root certificate */
        {
            oe_ec_public_key_t root_key;
            oe_ec_public_key_t cert_key;

            OE_TEST(oe_cert_get_ec_public_key(&root, &root_key) == OE_OK);

            oe_ec_public_key_free(&root_key);
            oe_ec_public_key_free(&cert_key);
        }

        /* Check that the keys are not identical for leaf and root */
        {
            oe_ec_public_key_t root_key;
            oe_ec_public_key_t leaf_key;
            bool equal;

            OE_TEST(oe_cert_get_ec_public_key(&root, &root_key) == OE_OK);
            OE_TEST(oe_cert_get_ec_public_key(&leaf, &leaf_key) == OE_OK);

            OE_TEST(
                oe_ec_public_key_equal(&root_key, &leaf_key, &equal) == OE_OK);
            OE_TEST(equal == false);

            oe_ec_public_key_free(&root_key);
            oe_ec_public_key_free(&leaf_key);
        }

        oe_cert_free(&root);
        oe_cert_free(&leaf);
        oe_cert_chain_free(&chain);
    }

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_key_from_bytes()
{
    printf("=== begin %s()()\n", __FUNCTION__);

    oe_result_t r;
    oe_ec_type_t ec_type = OE_EC_TYPE_SECP256R1;

    /* Test generating a key and then re-creating it from its bytes */
    oe_ec_private_key_t private_key = {0};
    oe_ec_public_key_t public_key = {0};
    oe_ec_public_key_t public_key2 = {0};
    uint8_t signature[1024];
    size_t signature_size = sizeof(signature);

    /* Load private key */
    r = oe_ec_private_key_read_pem(
        &private_key, private_key_pem, private_key_size + 1);
    OE_TEST(r == OE_OK);

    /* Load public key */
    r = oe_ec_public_key_read_pem(
        &public_key, public_key_pem, public_key_size + 1);
    OE_TEST(r == OE_OK);

    /* Create a second public key from the key bytes */
    r = oe_ec_public_key_from_coordinates(
        &public_key2, ec_type, x_data, x_size, y_data, y_size);
    OE_TEST(r == OE_OK);

    /* Sign data with private key */
    r = oe_ec_private_key_sign(
        &private_key,
        OE_HASH_TYPE_SHA256,
        &ALPHABET_HASH,
        sizeof(ALPHABET_HASH),
        signature,
        &signature_size);
    OE_TEST(r == OE_OK);

    /* Verify data with key created from bytes of original public key */
    r = oe_ec_public_key_verify(
        &public_key2,
        OE_HASH_TYPE_SHA256,
        &ALPHABET_HASH,
        sizeof(ALPHABET_HASH),
        signature,
        signature_size);
    OE_TEST(r == OE_OK);

    oe_ec_private_key_free(&private_key);
    oe_ec_public_key_free(&public_key);
    oe_ec_public_key_free(&public_key2);

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_cert_chain_read()
{
    printf("=== begin %s()\n", __FUNCTION__);

    oe_result_t r;
    oe_cert_chain_t chain;

    r = oe_cert_chain_read_pem(&chain, _CHAIN, strlen(_CHAIN) + 1);
    OE_TEST(r == OE_OK);

    oe_cert_chain_free(&chain);

    printf("=== passed %s()\n", __FUNCTION__);
}

typedef struct _extension
{
    const char* oid;
    size_t size;
    const uint8_t* data;
} Extension;

static const uint8_t _eccert_extensions_data0[] = {
    0x30, 0x16, 0x80, 0x14, 0xe5, 0xbb, 0x52, 0x8f, 0x80, 0xf9, 0xe3, 0x33,
    0xae, 0x19, 0xac, 0xfa, 0x63, 0x46, 0x78, 0x11, 0xf3, 0x61, 0xbb, 0xa4,
};

static const uint8_t _eccert_extensions_data1[] = {
    0x30, 0x4f, 0x30, 0x4d, 0xa0, 0x4b, 0xa0, 0x49, 0x86, 0x47, 0x68, 0x74,
    0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66,
    0x69, 0x63, 0x61, 0x74, 0x65, 0x73, 0x2e, 0x74, 0x72, 0x75, 0x73, 0x74,
    0x65, 0x64, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x73, 0x2e, 0x69,
    0x6e, 0x74, 0x65, 0x6c, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x49, 0x6e, 0x74,
    0x65, 0x6c, 0x53, 0x47, 0x58, 0x50, 0x43, 0x4b, 0x50, 0x72, 0x6f, 0x63,
    0x65, 0x73, 0x73, 0x6f, 0x72, 0x2e, 0x63, 0x72, 0x6c,

};

static const uint8_t _eccert_extensions_data2[] = {
    0x04, 0x14, 0xce, 0x29, 0xe9, 0x5e, 0xff, 0xe1, 0x97, 0x89, 0xe4,
    0x6d, 0x48, 0x3b, 0xb1, 0xf2, 0xde, 0xc6, 0x3b, 0xa4, 0xe5, 0x1f,
};

static const uint8_t _eccert_extensions_data3[] = {
    0x03,
    0x02,
    0x06,
    0xc0,
};

static const uint8_t _eccert_extensions_data4[] = {
    0x30,
    0x00,
};

static const uint8_t _eccert_extensions_data5[] = {
    0x30, 0x82, 0x01, 0xc1, 0x30, 0x1e, 0x06, 0x0a, 0x2a, 0x86, 0x48, 0x86,
    0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x01, 0x04, 0x10, 0x69, 0xc8, 0x8d, 0xe2,
    0x56, 0xc8, 0x58, 0x25, 0x37, 0x5e, 0x7b, 0x85, 0xe0, 0x10, 0xc9, 0x9a,
    0x30, 0x82, 0x01, 0x64, 0x06, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d,
    0x01, 0x0d, 0x01, 0x02, 0x30, 0x82, 0x01, 0x54, 0x30, 0x10, 0x06, 0x0b,
    0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x02,
    0x01, 0x04, 0x30, 0x10, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d,
    0x01, 0x0d, 0x01, 0x02, 0x02, 0x02, 0x01, 0x04, 0x30, 0x10, 0x06, 0x0b,
    0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x03, 0x02,
    0x01, 0x02, 0x30, 0x10, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d,
    0x01, 0x0d, 0x01, 0x02, 0x04, 0x02, 0x01, 0x04, 0x30, 0x10, 0x06, 0x0b,
    0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x05, 0x02,
    0x01, 0x01, 0x30, 0x11, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d,
    0x01, 0x0d, 0x01, 0x02, 0x06, 0x02, 0x02, 0x00, 0x80, 0x30, 0x10, 0x06,
    0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x07,
    0x02, 0x01, 0x00, 0x30, 0x10, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8,
    0x4d, 0x01, 0x0d, 0x01, 0x02, 0x08, 0x02, 0x01, 0x00, 0x30, 0x10, 0x06,
    0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x09,
    0x02, 0x01, 0x00, 0x30, 0x10, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8,
    0x4d, 0x01, 0x0d, 0x01, 0x02, 0x0a, 0x02, 0x01, 0x00, 0x30, 0x10, 0x06,
    0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x0b,
    0x02, 0x01, 0x00, 0x30, 0x10, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8,
    0x4d, 0x01, 0x0d, 0x01, 0x02, 0x0c, 0x02, 0x01, 0x00, 0x30, 0x10, 0x06,
    0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x0d,
    0x02, 0x01, 0x00, 0x30, 0x10, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8,
    0x4d, 0x01, 0x0d, 0x01, 0x02, 0x0e, 0x02, 0x01, 0x00, 0x30, 0x10, 0x06,
    0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x0f,
    0x02, 0x01, 0x00, 0x30, 0x10, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8,
    0x4d, 0x01, 0x0d, 0x01, 0x02, 0x10, 0x02, 0x01, 0x00, 0x30, 0x10, 0x06,
    0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x02, 0x11,
    0x02, 0x01, 0x05, 0x30, 0x1f, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf8,
    0x4d, 0x01, 0x0d, 0x01, 0x02, 0x12, 0x04, 0x10, 0x04, 0x04, 0x02, 0x04,
    0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30, 0x10, 0x06, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d,
    0x01, 0x03, 0x04, 0x02, 0x00, 0x00, 0x30, 0x14, 0x06, 0x0a, 0x2a, 0x86,
    0x48, 0x86, 0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x04, 0x04, 0x06, 0x00, 0x90,
    0x6e, 0xa1, 0x00, 0x00, 0x30, 0x0f, 0x06, 0x0a, 0x2a, 0x86, 0x48, 0x86,
    0xf8, 0x4d, 0x01, 0x0d, 0x01, 0x05, 0x0a, 0x01, 0x00,
};

static const Extension _eccert_extensions[] = {
    {
        .oid = "2.5.29.35",
        .size = 24,
        .data = _eccert_extensions_data0,
    },
    {
        .oid = "2.5.29.31",
        .size = 81,
        .data = _eccert_extensions_data1,
    },
    {
        .oid = "2.5.29.14",
        .size = 22,
        .data = _eccert_extensions_data2,
    },
    {
        .oid = "2.5.29.15",
        .size = 4,
        .data = _eccert_extensions_data3,
    },
    {
        .oid = "2.5.29.19",
        .size = 2,
        .data = _eccert_extensions_data4,
    },
    {
        .oid = "1.2.840.113741.1.13.1",
        .size = 453,
        .data = _eccert_extensions_data5,
    },
};

static void _test_cert_extensions(
    const char* cert_data,
    size_t cert_size,
    const Extension* extensions,
    size_t extensions_count,
    const char* test_oid)
{
    oe_cert_t cert;

    printf("=== begin %s()\n", __FUNCTION__);

    OE_TEST(oe_cert_read_pem(&cert, cert_data, cert_size) == OE_OK);

    /* Test finding extensions by OID */
    {
        for (size_t i = 0; i < extensions_count; i++)
        {
            const Extension* ext = &extensions[i];
            const char* oid = ext->oid;
            uint8_t data[4096];
            size_t size = sizeof(data);

            OE_TEST(oe_cert_find_extension(&cert, oid, data, &size) == OE_OK);
            OE_TEST(strcmp(oid, ext->oid) == 0);
            OE_TEST(size == ext->size);
            OE_TEST(memcmp(data, ext->data, size) == 0);
        }
    }

    /* Check for an unknown OID */
    if (!extensions)
    {
        oe_result_t r;
        uint8_t data[4096];
        size_t size = sizeof(data);

        r = oe_cert_find_extension(&cert, "1.2.3.4", data, &size);
        OE_TEST(r == OE_NOT_FOUND);
    }

    /* Find the extension with the given OID and check for OE_NOT_FOUND */
    if (!extensions)
    {
        oe_result_t r;
        uint8_t data[4096];
        size_t size = sizeof(data);

        r = oe_cert_find_extension(&cert, test_oid, data, &size);

        if (extensions)
            OE_TEST(r == OE_OK);
        else
            OE_TEST(r == OE_NOT_FOUND);
    }

    oe_cert_free(&cert);

    printf("=== passed %s()\n", __FUNCTION__);
}

static void _test_cert_with_extensions()
{
    /* Test a certificate with extensions */
    _test_cert_extensions(
        _CERT,
        strlen(_CERT) + 1,
        _eccert_extensions,
        OE_COUNTOF(_eccert_extensions),
        "1.2.840.113741.1.13.1");
}

static void _test_cert_without_extensions()
{
    /* Test a certificate without extensions */
    _test_cert_extensions(
        _CERT_WITHOUT_EXTENSIONS,
        strlen(_CERT_WITHOUT_EXTENSIONS) + 1,
        NULL,
        0,
        "2.5.29.35");
}

static const char _URL[] =
    "https://certificates.trustedservices.intel.com/IntelSGXPCKProcessor.crl";

static void _test_crl_distribution_points(void)
{
    oe_result_t r;
    oe_cert_t cert;
    const char** urls = NULL;
    size_t num_urls;
    size_t buffer_size = 0;

    printf("=== begin %s()\n", __FUNCTION__);

    r = oe_cert_read_pem(&cert, _SGX_CERT, strlen(_SGX_CERT) + 1);
    OE_TEST(r == OE_OK);

    r = oe_get_crl_distribution_points(
        &cert, &urls, &num_urls, NULL, &buffer_size);
    OE_TEST(r == OE_BUFFER_TOO_SMALL);

    {
        OE_ALIGNED(8) uint8_t buffer[buffer_size];

        r = oe_get_crl_distribution_points(
            &cert, &urls, &num_urls, buffer, &buffer_size);

        OE_TEST(num_urls == 1);
        OE_TEST(urls != NULL);
        OE_TEST(urls[0] != NULL);
        OE_TEST(strcmp(urls[0], _URL) == 0);

        printf("URL{%s}\n", urls[0]);

        OE_TEST(r == OE_OK);
    }

    r = oe_cert_free(&cert);
    OE_TEST(r == OE_OK);

    printf("=== passed %s()\n", __FUNCTION__);
}

void TestEC()
{
    OE_TEST(read_cert("../data/ec_cert_with_ext.pem", _CERT) == OE_OK);
    OE_TEST(
        read_cert("../data/Leafec.crt.pem", _CERT_WITHOUT_EXTENSIONS) == OE_OK);
    OE_TEST(
        read_cert("../data/ec_cert_crl_distribution.pem", _SGX_CERT) == OE_OK);
    OE_TEST(
        read_chains(
            "../data/Leafec.crt.pem",
            "../data/Intermediateec.crt.pem",
            "../data/Rootec.crt.pem",
            _CHAIN) == OE_OK);
    OE_TEST(read_key("../data/Rootec.key.pem", _PRIVATE_KEY) == OE_OK);
    OE_TEST(read_key("../data/Rootec.public.key", _PUBLIC_KEY) == OE_OK);
    OE_TEST(
        read_sign("../data/test_ec_signature", _SIGNATURE, &sign_size) ==
        OE_OK);
    OE_TEST(
        read_pem_key(
            "../data/Rootec.key.pem", private_key_pem, &private_key_size) ==
        OE_OK);
    OE_TEST(
        read_pem_key(
            "../data/Rootec.public.key", public_key_pem, &public_key_size) ==
        OE_OK);
    OE_TEST(
        read_coordinates(
            "../data/coordinates.bin", x_data, y_data, &x_size, &y_size) ==
        OE_OK);

    _test_cert_with_extensions();
    _test_cert_without_extensions();
    _test_crl_distribution_points();
    _test_sign_and_verify();
    _test_generate();
    _test_generate_from_private();
    _test_private_key_limits();
    _test_write_private();
    _test_write_public();
    _test_cert_methods();
    _test_key_from_bytes();
    _test_cert_chain_read();
}
