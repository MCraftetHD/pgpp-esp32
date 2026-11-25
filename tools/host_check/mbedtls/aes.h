#pragma once
#include <cstdint>
typedef struct { int dummy; } mbedtls_aes_context;
inline void mbedtls_aes_init(mbedtls_aes_context*) {}
inline void mbedtls_aes_free(mbedtls_aes_context*) {}
inline int mbedtls_aes_setkey_enc(mbedtls_aes_context*, const unsigned char*, unsigned int) { return 0; }
inline int mbedtls_aes_crypt_ecb(mbedtls_aes_context*, int, const unsigned char*, unsigned char*) { return 0; }
#define MBEDTLS_AES_ENCRYPT 1
