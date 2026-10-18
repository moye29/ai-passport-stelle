#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { size_t len; const unsigned char *p; } test_buf;
typedef struct mbedtls_x509_crt { test_buf issuer_raw; const unsigned char *der; size_t der_len; } mbedtls_x509_crt;
#define MBEDTLS_ERR_X509_ALLOC_FAILED (-0x2880)
void mbedtls_x509_crt_init(mbedtls_x509_crt *);
void mbedtls_x509_crt_free(mbedtls_x509_crt *);
int mbedtls_x509_crt_parse_der_nocopy(mbedtls_x509_crt *,const unsigned char *,size_t);
