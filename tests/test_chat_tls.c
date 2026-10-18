#include "chat_tls.h"
#include "chat_ca.h"
#include "mbedtls/ssl.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int parse_error, released;
void mbedtls_x509_crt_init(mbedtls_x509_crt *p) { memset(p,0,sizeof(*p)); }
void mbedtls_x509_crt_free(mbedtls_x509_crt *p) { assert(p); released++; }
int mbedtls_x509_crt_parse_der_nocopy(mbedtls_x509_crt *p,const unsigned char *d,size_t n)
{ if (parse_error) return parse_error; p->der=d;p->der_len=n;return 0; }
void mbedtls_ssl_conf_ca_cb(mbedtls_ssl_config *c,test_ca_callback fn,void *ctx)
{ c->callback=fn;c->context=ctx; }
void mbedtls_ssl_conf_authmode(mbedtls_ssl_config *c,int mode) { c->authmode=mode; }
int main(void)
{
    assert(chat_tls_attach(NULL)==ESP_ERR_INVALID_ARG);
    mbedtls_ssl_config cfg={0};assert(chat_tls_attach(&cfg)==ESP_OK);
    assert(cfg.callback && cfg.authmode==MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_x509_crt child={.issuer_raw={7,(const unsigned char *)"unknown"}},*out=(void *)1;
    assert(cfg.callback(cfg.context,&child,&out)==0 && out==NULL);
    for (size_t i=0;i<CHAT_CA_COUNT;i++) {
        child.issuer_raw.len=chat_ca_roots[i].subject_len;child.issuer_raw.p=chat_ca_roots[i].subject;
        assert(cfg.callback(cfg.context,&child,&out)==0 && out);
        assert(out->der && out->der_len>100);mbedtls_x509_crt_free(out);free(out);
    }
    parse_error=-0x2180;int before=released;
    assert(cfg.callback(cfg.context,&child,&out)==parse_error && out==NULL);
    assert(released==before+1);
    puts("TLS anchor selection, unknown issuer, parse failure and required verification: PASS");
    return 0;
}
