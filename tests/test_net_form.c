#include "net_form.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static net_form_err_t parse(const char *body, net_form_t *f) { return net_form_parse(body, strlen(body), f); }

static void test_picked_network(void)
{
    net_form_t f;
    assert(parse("ssid=Home+WiFi&ssid_manual=&pass=pa%26ss%20word1&tz=480", &f) == NET_FORM_OK);
    assert(strcmp(f.ssid, "Home WiFi") == 0);
    assert(strcmp(f.pass, "pa&ss word1") == 0);
    assert(f.tz_min == 480);
}

static void test_manual_name_overrides_and_utf8(void)
{
    net_form_t f;
    assert(parse("ssid=Other&ssid_manual=%E5%AE%B6%E9%87%8C&pass=&tz=-330", &f) == NET_FORM_OK);
    assert(strcmp(f.ssid, "\xE5\xAE\xB6\xE9\x87\x8C") == 0);  // Chinese SSID survives
    assert(f.pass[0] == '\0' && f.tz_min == -330);
}

static void test_validation(void)
{
    net_form_t f;
    assert(parse("ssid=&pass=12345678", &f) == NET_FORM_NO_SSID);
    assert(parse("pass=12345678", &f) == NET_FORM_NO_SSID);
    assert(parse("ssid=a&pass=1234567", &f) == NET_FORM_PASS_TOO_SHORT);
    assert(parse("ssid=a&pass=1234567890123456789012345678901234567890123456789012345678901234", &f) ==
           NET_FORM_PASS_TOO_LONG);
    assert(parse("ssid=123456789012345678901234567890123&pass=", &f) == NET_FORM_SSID_TOO_LONG);
    assert(parse("ssid=a&tz=999", &f) == NET_FORM_BAD_TZ);
    assert(parse("ssid=a&tz=8h", &f) == NET_FORM_BAD_TZ);
    assert(parse("ssid=a&tz=490", &f) == NET_FORM_BAD_TZ);  // not a quarter hour
    assert(parse("ssid=a", &f) == NET_FORM_OK && f.tz_min == NET_TZ_DEFAULT_MIN);
    assert(parse("ssid=%zz", &f) == NET_FORM_OK && strcmp(f.ssid, "%zz") == 0);  // bad escape kept literally
    assert(strlen(net_form_error_text(NET_FORM_PASS_TOO_SHORT)) > 0);
}

static void test_escape_and_tz(void)
{
    char out[64];
    net_html_escape("<a href=\"x\">'&'</a>", out, sizeof(out));
    assert(strcmp(out, "&lt;a href=&quot;x&quot;&gt;&#39;&amp;&#39;&lt;/a&gt;") == 0);
    char small[6];
    net_html_escape("a&b", small, sizeof(small));
    assert(strcmp(small, "a") == 0);  // never splits an entity
    char tz[16];
    net_tz_posix(480, tz, sizeof(tz));
    assert(strcmp(tz, "UTC-8") == 0);
    net_tz_posix(330, tz, sizeof(tz));
    assert(strcmp(tz, "UTC-5:30") == 0);
    net_tz_posix(-300, tz, sizeof(tz));
    assert(strcmp(tz, "UTC+5") == 0);
    net_tz_posix(0, tz, sizeof(tz));
    assert(strcmp(tz, "UTC+0") == 0);
}

int main(void)
{
    test_picked_network();
    test_manual_name_overrides_and_utf8();
    test_validation();
    test_escape_and_tz();
    puts("net form tests: PASS");
    return 0;
}
