#ifndef MBEDTLS_CONFIG_AGENTOS_H
#define MBEDTLS_CONFIG_AGENTOS_H

#define MBEDTLS_HAVE_ASM
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_IN_CONTENT_LEN 32768
#define MBEDTLS_SSL_OUT_CONTENT_LEN 8192
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_PADDING_PKCS7

#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_PLATFORM_SNPRINTF_MACRO mbedtls_platform_snprintf
#define MBEDTLS_PLATFORM_PRINTF_MACRO mbedtls_platform_printf
#define MBEDTLS_ENTROPY_HARDWARE_ALT

#include <stddef.h>

void *mbedtls_calloc(size_t n, size_t size);
void mbedtls_free(void *ptr);
int mbedtls_platform_snprintf(char *s, size_t n, const char *fmt, ...);
int mbedtls_platform_printf(const char *fmt, ...);
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);

#endif
