/*
 * Copyright 1999-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include "internal/cryptlib.h"
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/hmac.h>
#include "crypto/evp.h"
#include "evp_local.h"

/* set this to print out info about the keygen algorithm */
/* #define OPENSSL_DEBUG_PKCS5V2 */

#ifdef OPENSSL_DEBUG_PKCS5V2
static void h__dump(const unsigned char *p, int len);
#endif

int PKCS5_PBKDF2_HMAC(const char *pass, int passlen,
                      const unsigned char *salt, int saltlen, int iter,
                      const EVP_MD *digest, int keylen, unsigned char *out)
{
    const char *empty = "";
    int rv = 1;
    EVP_KDF_CTX *kctx;

    /* Keep documented behaviour. */
    if (pass == NULL) {
        pass = empty;
        passlen = 0;
    } else if (passlen == -1) {
        passlen = strlen(pass);
    }
    if (salt == NULL && saltlen == 0)
        salt = (unsigned char *)empty;

    kctx = EVP_KDF_CTX_new_id(EVP_KDF_PBKDF2);
    if (kctx == NULL)
        return 0;
    if (EVP_KDF_ctrl(kctx, EVP_KDF_CTRL_SET_PASS, pass, (size_t)passlen) != 1
            || EVP_KDF_ctrl(kctx, EVP_KDF_CTRL_SET_SALT,
                            salt, (size_t)saltlen) != 1
            || EVP_KDF_ctrl(kctx, EVP_KDF_CTRL_SET_ITER, iter) != 1
            || EVP_KDF_ctrl(kctx, EVP_KDF_CTRL_SET_MD, digest) != 1
            || EVP_KDF_derive(kctx, out, keylen) != 1)
        rv = 0;

    EVP_KDF_CTX_free(kctx);

# ifdef OPENSSL_DEBUG_PKCS5V2
    fprintf(stderr, "Password:\n");
    h__dump(pass, passlen);
    fprintf(stderr, "Salt:\n");
    h__dump(salt, saltlen);
    fprintf(stderr, "Iteration count %d\n", iter);
    fprintf(stderr, "Key:\n");
    h__dump(out, keylen);
# endif
    return rv;
}

int PKCS5_PBKDF2_HMAC_SHA1(const char *pass, int passlen,
                           const unsigned char *salt, int saltlen, int iter,
                           int keylen, unsigned char *out)
{
    return PKCS5_PBKDF2_HMAC(pass, passlen, salt, saltlen, iter, EVP_sha1(),
                             keylen, out);
}

/*
 * Now the key derivation function itself. This is a bit evil because it has
 * to check the ASN1 parameters are valid: and there are quite a few of
 * them...
 */

int PKCS5_v2_PBE_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass, int passlen,
                          ASN1_TYPE *param, const EVP_CIPHER *c,
                          const EVP_MD *md, int en_de)
{
    PBE2PARAM *pbe2 = NULL;
    const EVP_CIPHER *cipher;
    EVP_PBE_KEYGEN *kdf;

    int rv = 0;

    pbe2 = ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(PBE2PARAM), param);
    if (pbe2 == NULL) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN, EVP_R_DECODE_ERROR);
        goto err;
    }

    /* See if we recognise the key derivation function */
    if (!EVP_PBE_find(EVP_PBE_TYPE_KDF, OBJ_obj2nid(pbe2->keyfunc->algorithm),
                        NULL, NULL, &kdf)) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN,
               EVP_R_UNSUPPORTED_KEY_DERIVATION_FUNCTION);
        goto err;
    }

    /*
     * lets see if we recognise the encryption algorithm.
     */

    cipher = EVP_get_cipherbyobj(pbe2->encryption->algorithm);

    if (!cipher) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN, EVP_R_UNSUPPORTED_CIPHER);
        goto err;
    }

    /* Fixup cipher based on AlgorithmIdentifier */
    if (!EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, en_de))
        goto err;
    if (EVP_CIPHER_asn1_to_param(ctx, pbe2->encryption->parameter) < 0) {
        EVPerr(EVP_F_PKCS5_V2_PBE_KEYIVGEN, EVP_R_CIPHER_PARAMETER_ERROR);
        goto err;
    }
    rv = kdf(ctx, pass, passlen, pbe2->keyfunc->parameter, NULL, NULL, en_de);
 err:
    PBE2PARAM_free(pbe2);
    return rv;
}

int PKCS5_v2_PBKDF2_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass,
                             int passlen, ASN1_TYPE *param,
                             const EVP_CIPHER *c, const EVP_MD *md, int en_de)
{
    unsigned char *salt, key[EVP_MAX_KEY_LENGTH];
    int saltlen, iter;
    int rv = 0;
    unsigned int keylen = 0;
    int prf_nid, hmac_md_nid;
    PBKDF2PARAM *kdf = NULL;
    const EVP_MD *prfmd;

    if (EVP_CIPHER_CTX_cipher(ctx) == NULL) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_NO_CIPHER_SET);
        goto err;
    }
    keylen = EVP_CIPHER_CTX_key_length(ctx);
    OPENSSL_assert(keylen <= sizeof(key));

    /* Decode parameter */

    kdf = ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(PBKDF2PARAM), param);

    if (kdf == NULL) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_DECODE_ERROR);
        goto err;
    }

    keylen = EVP_CIPHER_CTX_key_length(ctx);

    /* Now check the parameters of the kdf */

    if (kdf->keylength && (ASN1_INTEGER_get(kdf->keylength) != (int)keylen)) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_KEYLENGTH);
        goto err;
    }

    if (kdf->prf)
        prf_nid = OBJ_obj2nid(kdf->prf->algorithm);
    else
        prf_nid = NID_hmacWithSHA1;

    if (!EVP_PBE_find(EVP_PBE_TYPE_PRF, prf_nid, NULL, &hmac_md_nid, 0)) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_PRF);
        goto err;
    }

    prfmd = EVP_get_digestbynid(hmac_md_nid);
    if (prfmd == NULL) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_PRF);
        goto err;
    }

    if (kdf->salt->type != V_ASN1_OCTET_STRING) {
        EVPerr(EVP_F_PKCS5_V2_PBKDF2_KEYIVGEN, EVP_R_UNSUPPORTED_SALT_TYPE);
        goto err;
    }

    /* it seems that its all OK */
    salt = kdf->salt->value.octet_string->data;
    saltlen = kdf->salt->value.octet_string->length;
    iter = ASN1_INTEGER_get(kdf->iter);
    if (!PKCS5_PBKDF2_HMAC(pass, passlen, salt, saltlen, iter, prfmd,
                           keylen, key))
        goto err;
    rv = EVP_CipherInit_ex(ctx, NULL, NULL, key, NULL, en_de);
 err:
    OPENSSL_cleanse(key, keylen);
    PBKDF2PARAM_free(kdf);
    return rv;
}

# ifdef OPENSSL_DEBUG_PKCS5V2
static void h__dump(const unsigned char *p, int len)
{
    for (; len--; p++)
        fprintf(stderr, "%02X ", *p);
    fprintf(stderr, "\n");
}
# endif
