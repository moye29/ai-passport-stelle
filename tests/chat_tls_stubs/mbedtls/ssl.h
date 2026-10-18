#pragma once
#include "x509_crt.h"
#define MBEDTLS_SSL_VERIFY_REQUIRED 2
typedef int (*test_ca_callback)(void *,const mbedtls_x509_crt *,mbedtls_x509_crt **);
typedef struct { test_ca_callback callback; void *context; int authmode; } mbedtls_ssl_config;
void mbedtls_ssl_conf_ca_cb(mbedtls_ssl_config *,test_ca_callback,void *);
void mbedtls_ssl_conf_authmode(mbedtls_ssl_config *,int);
