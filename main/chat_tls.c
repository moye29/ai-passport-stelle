#include "chat_tls.h"
#include "chat_ca.h"
#include <stdlib.h>
#include <string.h>
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

static void release_candidates(mbedtls_x509_crt *roots)
{
    if (roots) { mbedtls_x509_crt_free(roots); free(roots); }
}

static int candidates(void *ctx, const mbedtls_x509_crt *child, mbedtls_x509_crt **out)
{
    (void)ctx;
    *out=NULL;
    mbedtls_x509_crt *roots=NULL;
    for (size_t i=0; i<CHAT_CA_COUNT; i++) {
        const chat_ca_t *ca=&chat_ca_roots[i];
        if (ca->subject_len!=child->issuer_raw.len ||
            memcmp(ca->subject,child->issuer_raw.p,ca->subject_len)!=0) continue;
        if (!roots) {
            roots=calloc(1,sizeof(*roots));
            if (!roots) return MBEDTLS_ERR_X509_ALLOC_FAILED;
            mbedtls_x509_crt_init(roots);
        }
        // The source DER remains immutable in Flash for the application's life.
        // Standard Mbed TLS parsing owns its parsed fields, but not these bytes.
        int err=mbedtls_x509_crt_parse_der_nocopy(roots,ca->der,ca->der_len);
        if (err) { release_candidates(roots); return err; }
    }
    // Mbed TLS takes ownership of the candidate list and frees it after use.
    // Unknown issuers return no anchors; normal chain verification then fails.
    *out=roots;
    return 0;
}

esp_err_t chat_tls_attach(void *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    mbedtls_ssl_config *cfg=config;
    mbedtls_ssl_conf_ca_cb(cfg,candidates,NULL);
    mbedtls_ssl_conf_authmode(cfg,MBEDTLS_SSL_VERIFY_REQUIRED);
    // Leave signatures, validity, chain constraints and hostname verification
    // to Mbed TLS. No verification flags or errors are cleared here.
    return ESP_OK;
}
