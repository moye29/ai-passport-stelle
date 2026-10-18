// main/net.c - Wi-Fi setup portal (SoftAP + captive page) and network time.
//
// One task owns the radio. The platform loop sends commands and reads a status
// snapshot; HTTP handlers run in the httpd task and only touch the scan list
// and status under s_lock. The home network password is stored in NVS
// namespace "net" and is never logged.
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "dns_server.h"  // needs esp_netif.h first
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "net_form.h"
#include "focus_log.h"
#include "nvs.h"

static const char *TAG = "net";

#define SCAN_MAX 16
#define JOIN_ATTEMPT_MS 12000
#define JOIN_ATTEMPTS 2
#define SNTP_TIMEOUT_MS 15000
#define FORM_MAX 640
#define PORTAL_URL "http://192.168.4.1/"
#define BIT_GOT_IP BIT0
#define BIT_FAILED BIT1

typedef enum { CMD_PORTAL_START, CMD_PORTAL_STOP, CMD_SYNC, CMD_JOIN, CMD_CHAT_ON, CMD_CHAT_OFF } cmd_type_t;
typedef struct {
    cmd_type_t type;
    net_form_t form;
} cmd_t;

typedef enum { FAIL_NONE = 0, FAIL_PASSWORD, FAIL_NOT_FOUND, FAIL_OTHER } fail_kind_t;

typedef struct {
    char ssid[NET_SSID_MAX + 1];
    int8_t rssi;
    bool open;
} scan_ap_t;

static QueueHandle_t s_cmds;
static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_events;
static net_status_t s_status;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_http;
static dns_server_handle_t s_dns;
static bool s_wifi_on, s_portal;
static bool s_chat_lease;
#define BIT_CHAT_READY BIT2
#define BIT_CHAT_IDLE BIT3
static volatile uint8_t s_last_reason;
static fail_kind_t s_fail;
static bool s_time_ready;
static uint32_t s_time_sod;
static int32_t s_time_day;   // local day number of the synced time
static int s_tz_min = NET_TZ_DEFAULT_MIN;
static scan_ap_t s_scan[SCAN_MAX];
static int s_scan_n;

/* ---------- status ---------- */

static void lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

static void set_phase(net_phase_t phase)
{
    lock();
    s_status.phase = phase;
    unlock();
}

void net_get_status(net_status_t *out)
{
    if (!s_lock) {
        memset(out, 0, sizeof(*out));
        return;
    }
    lock();
    *out = s_status;
    unlock();
}

bool net_take_time(uint32_t *seconds_of_day, int32_t *day)
{
    if (!s_lock) return false;
    lock();
    bool ready = s_time_ready;
    if (ready) {
        *seconds_of_day = s_time_sod;
        *day = s_time_day;
    }
    s_time_ready = false;
    unlock();
    return ready;
}

/* ---------- saved network (NVS "net") ---------- */

static bool creds_load(char *ssid, char *pass, int *tz)
{
    nvs_handle_t h;
    if (nvs_open("net", NVS_READONLY, &h) != ESP_OK) return false;
    size_t sl = NET_SSID_MAX + 1, pl = NET_PASS_MAX + 1;
    int32_t t = NET_TZ_DEFAULT_MIN;
    bool ok = nvs_get_str(h, "ssid", ssid, &sl) == ESP_OK && nvs_get_str(h, "pass", pass, &pl) == ESP_OK && ssid[0];
    if (nvs_get_i32(h, "tz", &t) != ESP_OK || !net_tz_valid((int)t)) t = NET_TZ_DEFAULT_MIN;
    nvs_close(h);
    *tz = (int)t;
    return ok;
}

static bool creds_save(const net_form_t *f)
{
    nvs_handle_t h;
    if (nvs_open("net", NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_str(h, "ssid", f->ssid) == ESP_OK && nvs_set_str(h, "pass", f->pass) == ESP_OK &&
              nvs_set_i32(h, "tz", f->tz_min) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

/* ---------- radio ---------- */

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && (id == WIFI_EVENT_AP_STACONNECTED || id == WIFI_EVENT_AP_STADISCONNECTED)) {
        wifi_sta_list_t list;
        bool joined = id == WIFI_EVENT_AP_STACONNECTED ||
                      (esp_wifi_ap_get_sta_list(&list) == ESP_OK && list.num > 0);
        lock();
        s_status.phone_joined = joined;
        unlock();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_last_reason = ((wifi_event_sta_disconnected_t *)data)->reason;
        xEventGroupClearBits(s_events, BIT_CHAT_READY);
        xEventGroupSetBits(s_events, BIT_FAILED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, BIT_GOT_IP);
    }
}

static void radio_off(void)
{
    if (s_wifi_on) {
        esp_wifi_stop();
        s_wifi_on = false;
    }
}

static void radio_on(wifi_mode_t mode)
{
    radio_off();
    esp_wifi_set_mode(mode);
}

static void radio_start(void)
{
    if (esp_wifi_start() == ESP_OK) s_wifi_on = true;
}

static int cmp_rssi(const void *a, const void *b)
{
    return ((const scan_ap_t *)b)->rssi - ((const scan_ap_t *)a)->rssi;
}

// Blocking scan; keeps the strongest entry per name, strongest first.
static void scan(void)
{
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return;
    uint16_t n = 24;
    wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
    if (!recs) {
        esp_wifi_clear_ap_list();
        return;
    }
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) n = 0;
    lock();
    s_scan_n = 0;
    for (int i = 0; i < n; i++) {
        const char *name = (const char *)recs[i].ssid;
        if (!name[0]) continue;
        int found = -1;
        for (int j = 0; j < s_scan_n; j++)
            if (strcmp(s_scan[j].ssid, name) == 0) found = j;
        if (found >= 0) {
            if (recs[i].rssi > s_scan[found].rssi) s_scan[found].rssi = recs[i].rssi;
        } else if (s_scan_n < SCAN_MAX) {
            snprintf(s_scan[s_scan_n].ssid, sizeof(s_scan[0].ssid), "%s", name);
            s_scan[s_scan_n].rssi = recs[i].rssi;
            s_scan[s_scan_n].open = recs[i].authmode == WIFI_AUTH_OPEN;
            s_scan_n++;
        }
    }
    qsort(s_scan, (size_t)s_scan_n, sizeof(s_scan[0]), cmp_rssi);
    unlock();
    free(recs);
    ESP_LOGI(TAG, "scan: %d networks", s_scan_n);
}

// Join `ssid` on the STA interface (the AP may stay up). Sets s_fail on failure.
static bool sta_join(const char *ssid, const char *pass)
{
    wifi_config_t c = {0};
    snprintf((char *)c.sta.ssid, sizeof(c.sta.ssid), "%s", ssid);
    snprintf((char *)c.sta.password, sizeof(c.sta.password), "%s", pass);
    c.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
    c.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    c.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    c.sta.pmf_cfg.capable = true;
    if (esp_wifi_set_config(WIFI_IF_STA, &c) != ESP_OK) {
        s_fail = FAIL_OTHER;
        return false;
    }
    for (int attempt = 0; attempt < JOIN_ATTEMPTS; attempt++) {
        xEventGroupClearBits(s_events, BIT_GOT_IP | BIT_FAILED);
        s_last_reason = 0;
        if (esp_wifi_connect() != ESP_OK) break;
        EventBits_t bits = xEventGroupWaitBits(s_events, BIT_GOT_IP | BIT_FAILED, pdTRUE, pdFALSE,
                                               pdMS_TO_TICKS(JOIN_ATTEMPT_MS));
        if (bits & BIT_GOT_IP) {
            s_fail = FAIL_NONE;
            return true;
        }
        uint8_t r = s_last_reason;
        esp_wifi_disconnect();
        if (r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            s_fail = FAIL_PASSWORD;
            return false;
        }
        s_fail = r == WIFI_REASON_NO_AP_FOUND ? FAIL_NOT_FOUND : FAIL_OTHER;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return false;
}

/* ---------- setup page (captive portal) ---------- */

static const char PAGE_HEAD[] =
    "<!doctype html><html lang=\"zh\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Stelle Wi-Fi</title><style>"
    "body{margin:0;background:#dddbe6;font:16px -apple-system,system-ui,sans-serif;color:#262833}"
    ".card{margin:24px 16px;padding:22px 20px;background:#f2f1f7;border:2px solid #262833;border-radius:14px}"
    "h1{margin:0 0 6px;font-size:26px}.sub{margin:0 0 16px;color:#7e7d91}"
    "select,input{box-sizing:border-box;width:100%;margin:0 0 12px;padding:13px 14px;font-size:17px;"
    "border:1.5px solid #9091a2;border-radius:12px;background:#fff;color:#262833}"
    "label{display:block;color:#7e7d91;font-size:14px;margin:0 0 4px}"
    ".go,.btn{display:block;box-sizing:border-box;width:100%;padding:14px;margin:4px 0 12px;font-size:18px;"
    "font-weight:600;text-align:center;border-radius:12px;text-decoration:none}"
    ".go{border:2px solid #262833;background:#f5c15d;color:#262833}"
    ".btn{border:1.5px solid #9091a2;background:#e3e3ee;color:#262833}"
    ".err{padding:10px 12px;border-radius:10px;background:#fbe3a8;margin:0 0 14px}"
    ".hint{color:#7e7d91;font-size:14px;margin:4px 0 0}"
    ".show{display:flex;align-items:center;gap:8px;color:#262833;font-size:15px;margin:-4px 0 14px}"
    ".show input{width:20px;height:20px;margin:0;padding:0}"
    "</style></head><body>";

static void send(httpd_req_t *req, const char *s) { httpd_resp_sendstr_chunk(req, s); }

static void send_escaped(httpd_req_t *req, const char *s)
{
    char buf[NET_SSID_MAX * 6 + 1];
    net_html_escape(s, buf, sizeof(buf));
    send(req, buf);
}

static const char *bars(int rssi) { return rssi > -60 ? "强" : rssi > -75 ? "中" : "弱"; }

static void send_form(httpd_req_t *req, const char *error)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    send(req, PAGE_HEAD);
    send(req, "<form class=\"card\" method=\"post\" action=\"/save\"><h1>连接 Wi-Fi</h1>"
              "<p class=\"sub\">请选择 2.4 GHz Wi-Fi，并输入密码。</p>");
    if (error && error[0]) {
        send(req, "<p class=\"err\">");
        send(req, error);
        send(req, "</p>");
    }
    send(req, "<select name=\"ssid\" id=\"ssid\">");
    char line[96];
    lock();
    int n = s_scan_n;
    scan_ap_t list[SCAN_MAX];
    memcpy(list, s_scan, sizeof(list));
    unlock();
    for (int i = 0; i < n; i++) {  // strongest first; the first one is preselected
        send(req, i == 0 ? "<option selected value=\"" : "<option value=\"");
        send_escaped(req, list[i].ssid);
        send(req, "\">");
        send_escaped(req, list[i].ssid);
        snprintf(line, sizeof(line), "　·　%s%s</option>", bars(list[i].rssi), list[i].open ? "　开放" : "");
        send(req, line);
    }
    send(req, n ? "<option value=\"\">其他网络…</option>" : "<option selected value=\"\">其他网络…</option>");
    send(req, "</select><input name=\"ssid_manual\" id=\"manual\" placeholder=\"输入 Wi-Fi 名称\" maxlength=\"32\" "
              "autocapitalize=\"off\" style=\"display:none\">"
              "<input name=\"pass\" id=\"pw\" type=\"password\" placeholder=\"Wi-Fi 密码（开放网络留空）\" maxlength=\"63\" "
              "autocomplete=\"current-password\" autocapitalize=\"off\" autocorrect=\"off\" spellcheck=\"false\">"
              "<label class=\"show\"><input type=\"checkbox\" id=\"showpw\">显示密码</label>"
              "<label for=\"tz\">时区</label><select name=\"tz\" id=\"tz\">");
    static const short zones[] = {-720, -660, -600, -540, -480, -420, -360, -300, -240, -180, -120, -60, 0, 60, 120,
                                  180, 240, 300, 330, 345, 360, 420, 480, 540, 570, 600, 660, 720, 780, 840};
    for (size_t i = 0; i < sizeof(zones) / sizeof(zones[0]); i++) {
        int z = zones[i], a = abs(z);
        snprintf(line, sizeof(line), "<option value=\"%d\"%s>UTC%c%d%s%s</option>", z, z == s_tz_min ? " selected" : "",
                 z < 0 ? '-' : '+', a / 60, a % 60 ? (a % 60 == 30 ? ":30" : ":45") : "",
                 z == 480 ? "（北京时间）" : "");
        send(req, line);
    }
    send(req, "</select><button class=\"go\" type=\"submit\">开始连接</button>"
              "<a class=\"btn\" href=\"/scan\">重新扫描 Wi-Fi</a>"
              "<p>已选网络：<b id=\"cur\"></b></p>"
              "<p class=\"hint\">已按信号强度排序，请选择要连接的网络。</p></form><script>"
              "var s=document.getElementById('ssid'),m=document.getElementById('manual'),c=document.getElementById('cur');"
              "function p(){m.style.display=s.value?'none':'block';c.textContent=s.value||m.value||'—'}"
              "s.onchange=p;m.oninput=p;p();"
              "var w=document.getElementById('pw');"
              "document.getElementById('showpw').onchange=function(){w.type=this.checked?'text':'password'};"
              "</script></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
}

static const char *fail_text(void)
{
    switch (s_fail) {
    case FAIL_PASSWORD: return "密码不正确，请重新输入。";
    case FAIL_NOT_FOUND: return "找不到该网络。请确认是 2.4 GHz Wi-Fi 且在附近。";
    case FAIL_OTHER: return "连接失败，请重试。";
    default: return "";
    }
}

static esp_err_t on_root(httpd_req_t *req)
{
    send_form(req, s_status.phase == NET_PHASE_JOIN_FAILED ? fail_text() : NULL);
    return ESP_OK;
}

static esp_err_t on_scan(httpd_req_t *req)
{
    scan();
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const char *form_error_text(net_form_err_t e)
{
    switch (e) {
    case NET_FORM_NO_SSID: return "请选择或输入 Wi-Fi 名称。";
    case NET_FORM_SSID_TOO_LONG: return "Wi-Fi 名称过长（最多 32 字节）。";
    case NET_FORM_PASS_TOO_SHORT: return "Wi-Fi 密码至少 8 位；开放网络请留空。";
    case NET_FORM_PASS_TOO_LONG: return "密码过长（最多 63 位）。";
    case NET_FORM_BAD_TZ: return "请选择时区。";
    default: return "";
    }
}

static esp_err_t on_save(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len > FORM_MAX) {
        send_form(req, "提交内容无效，请重试。");
        return ESP_OK;
    }
    char body[FORM_MAX + 1];
    size_t got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, body + got, req->content_len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) return ESP_FAIL;
        got += (size_t)r;
    }
    body[got] = '\0';
    cmd_t cmd = {.type = CMD_JOIN};
    net_form_err_t err = net_form_parse(body, got, &cmd.form);
    memset(body, 0, sizeof(body));  // drop the plain-text password copy
    if (err != NET_FORM_OK) {
        send_form(req, form_error_text(err));
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send(req, PAGE_HEAD);
    send(req, "<div class=\"card\"><h1>正在连接…</h1><p>Passport 正在连接 <b>");
    send_escaped(req, cmd.form.ssid);
    send(req, "</b>。请看设备屏幕：连接成功后它会自动校准时间并进入主页，此热点随之关闭。</p>"
              "<p class=\"hint\">若屏幕提示失败，请重新打开 192.168.4.1 再试。</p></div></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    xQueueSend(s_cmds, &cmd, 0);
    memset(&cmd, 0, sizeof(cmd));
    return ESP_OK;
}

// Everything else (OS captive-portal probes included) is sent to the setup page.
static esp_err_t on_404(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", PORTAL_URL);
    httpd_resp_send(req, "Redirect to the Stelle setup page", HTTPD_RESP_USE_STRLEN);  // iOS needs a body
    return ESP_OK;
}

static void http_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 7;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 6144;
    if (httpd_start(&s_http, &cfg) != ESP_OK) {
        s_http = NULL;
        return;
    }
    static const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = on_root};
    static const httpd_uri_t rescan = {.uri = "/scan", .method = HTTP_GET, .handler = on_scan};
    static const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = on_save};
    httpd_register_uri_handler(s_http, &root);
    httpd_register_uri_handler(s_http, &rescan);
    httpd_register_uri_handler(s_http, &save);
    httpd_register_err_handler(s_http, HTTPD_404_NOT_FOUND, on_404);
}

static void portal_stop(void)
{
    if (s_dns) stop_dns_server(s_dns);
    if (s_http) httpd_stop(s_http);
    s_dns = NULL;
    s_http = NULL;
    s_portal = false;
    radio_off();
    lock();
    s_status.phone_joined = false;
    unlock();
}

static void portal_start(void)
{
    if (s_portal) return;
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    lock();
    snprintf(s_status.ap_ssid, sizeof(s_status.ap_ssid), "Stelle-%02X%02X", mac[4], mac[5]);
    snprintf(s_status.ap_pass, sizeof(s_status.ap_pass), "%08lu", (unsigned long)(esp_random() % 100000000UL));
    snprintf(s_status.ap_qr, sizeof(s_status.ap_qr), "WIFI:T:WPA;S:%s;P:%s;;", s_status.ap_ssid, s_status.ap_pass);
    s_status.phase = NET_PHASE_STARTING;
    s_status.phone_joined = false;
    s_status.message[0] = '\0';
    unlock();
    s_fail = FAIL_NONE;

    radio_on(WIFI_MODE_STA);  // scan first so the page can list networks
    radio_start();
    scan();
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    wifi_config_t ap = {0};
    lock();
    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s", s_status.ap_ssid);
    snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", s_status.ap_pass);
    unlock();
    ap.ap.ssid_len = (uint8_t)strlen((char *)ap.ap.ssid);
    ap.ap.max_connection = 2;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.ap.channel = 1;
    esp_wifi_set_config(WIFI_IF_AP, &ap);

    // DHCP option 114 tells modern phones where the captive page is.
    esp_netif_dhcps_stop(s_ap_netif);
    esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, (void *)PORTAL_URL,
                           strlen(PORTAL_URL));
    esp_netif_dhcps_start(s_ap_netif);

    http_start();
    dns_server_config_t dns = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    s_dns = start_dns_server(&dns);
    s_portal = true;
    set_phase(NET_PHASE_PORTAL);
    ESP_LOGI(TAG, "setup hotspot %s up", s_status.ap_ssid);
}

/* ---------- network time ---------- */

static void sync_time(void)
{
    char ssid[NET_SSID_MAX + 1] = {0}, pass[NET_PASS_MAX + 1] = {0};
    int tz = NET_TZ_DEFAULT_MIN;
    if (s_portal || s_chat_lease) return;  // never while a foreground service owns the connection
    if (!creds_load(ssid, pass, &tz)) {
        set_phase(NET_PHASE_IDLE);
        return;
    }
    s_tz_min = tz;
    lock();
    snprintf(s_status.sta_ssid, sizeof(s_status.sta_ssid), "%s", ssid);
    s_status.phase = NET_PHASE_CONNECTING;
    unlock();
    radio_on(WIFI_MODE_STA);
    radio_start();
    bool joined = sta_join(ssid, pass);
    memset(pass, 0, sizeof(pass));
    if (!joined) {
        radio_off();
        set_phase(NET_PHASE_OFFLINE);
        ESP_LOGW(TAG, "saved network unreachable (reason %u)", s_last_reason);
        return;
    }
    set_phase(NET_PHASE_SYNCING);
    char posix[16];
    net_tz_posix(tz, posix, sizeof(posix));
    setenv("TZ", posix, 1);
    tzset();
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        3, ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org"));
    bool ok = esp_netif_sntp_init(&cfg) == ESP_OK && esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SNTP_TIMEOUT_MS)) == ESP_OK;
    esp_netif_sntp_deinit();
    if (ok) {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        lock();
        s_time_sod = (uint32_t)(t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec);
        s_time_day = date_to_day(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        s_time_ready = true;
        unlock();
        ESP_LOGI(TAG, "time synced %02d:%02d:%02d (%s)", t.tm_hour, t.tm_min, t.tm_sec, posix);
    }
    esp_wifi_disconnect();
    radio_off();  // radio stays off between syncs
    set_phase(ok ? NET_PHASE_SYNCED : NET_PHASE_OFFLINE);
}

static void join_from_form(const net_form_t *f)
{
    lock();
    snprintf(s_status.sta_ssid, sizeof(s_status.sta_ssid), "%s", f->ssid);
    s_status.message[0] = '\0';
    s_status.phase = NET_PHASE_JOINING;
    unlock();
    if (sta_join(f->ssid, f->pass) && creds_save(f)) {
        s_tz_min = f->tz_min;
        lock();
        s_status.has_credentials = true;
        unlock();
        ESP_LOGI(TAG, "network saved");
        portal_stop();
        sync_time();
        return;
    }
    const char *why = s_fail == FAIL_PASSWORD   ? "Wrong password"
                    : s_fail == FAIL_NOT_FOUND ? "Network not found (2.4 GHz?)"
                                               : "Could not connect";
    lock();
    snprintf(s_status.message, sizeof(s_status.message), "%s. Try again.", why);
    s_status.phase = NET_PHASE_JOIN_FAILED;
    unlock();
    ESP_LOGW(TAG, "join failed (reason %u)", s_last_reason);
}

static void net_task(void *arg)
{
    (void)arg;
    cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_cmds, &cmd, portMAX_DELAY) != pdTRUE) continue;
        switch (cmd.type) {
        case CMD_PORTAL_START:
            xEventGroupClearBits(s_events, BIT_CHAT_READY);
            s_chat_lease = false;
            portal_start(); break;
        case CMD_PORTAL_STOP:
            if (s_portal) {
                portal_stop();
                set_phase(NET_PHASE_IDLE);
            }
            break;
        case CMD_SYNC: sync_time(); break;
        case CMD_JOIN: join_from_form(&cmd.form); break;
        case CMD_CHAT_ON: {
            xEventGroupClearBits(s_events, BIT_CHAT_IDLE);
            char ssid[NET_SSID_MAX+1] = {0}, pass[NET_PASS_MAX+1] = {0};
            int tz;
            if (!s_portal && creds_load(ssid, pass, &tz)) {
                s_chat_lease = true;
                radio_on(WIFI_MODE_STA);
                radio_start();
                if (sta_join(ssid, pass)) xEventGroupSetBits(s_events, BIT_CHAT_READY);
                else { s_chat_lease = false; radio_off(); xEventGroupSetBits(s_events, BIT_CHAT_IDLE); }
            }
            memset(pass, 0, sizeof(pass));
            break;
        }
        case CMD_CHAT_OFF:
            xEventGroupClearBits(s_events, BIT_CHAT_READY);
            if (s_chat_lease) { s_chat_lease = false; radio_off(); }
            xEventGroupSetBits(s_events, BIT_CHAT_IDLE);
            break;
        }
        memset(&cmd, 0, sizeof(cmd));
    }
}

/* ---------- API ---------- */

bool net_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    s_cmds = xQueueCreate(4, sizeof(cmd_t));
    if (!s_lock || !s_events || !s_cmds) return false;
    xEventGroupSetBits(s_events, BIT_CHAT_IDLE);
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);
    if (esp_netif_init() != ESP_OK) return false;
    esp_err_t e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;
    s_ap_netif = esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) return false;
    esp_wifi_set_storage(WIFI_STORAGE_RAM);  // credentials live in our own NVS namespace
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);

    char ssid[NET_SSID_MAX + 1], pass[NET_PASS_MAX + 1];
    int tz;
    s_status.has_credentials = creds_load(ssid, pass, &tz);
    memset(pass, 0, sizeof(pass));
    if (s_status.has_credentials) {
        s_tz_min = tz;
        snprintf(s_status.sta_ssid, sizeof(s_status.sta_ssid), "%s", ssid);
    }
    return xTaskCreate(net_task, "net", 6144, NULL, 3, NULL) == pdPASS;
}

static void post(cmd_type_t type)
{
    if (!s_cmds) return;
    cmd_t cmd = {.type = type};
    xQueueSend(s_cmds, &cmd, 0);
}

void net_start_portal(void) { post(CMD_PORTAL_START); }
void net_stop_portal(void) { post(CMD_PORTAL_STOP); }
void net_sync_time(void) { post(CMD_SYNC); }
bool net_chat_acquire(void)
{
    if (!s_cmds) return false;
    xEventGroupClearBits(s_events, BIT_CHAT_READY);
    cmd_t cmd = {.type = CMD_CHAT_ON};
    return xQueueSend(s_cmds, &cmd, 0) == pdTRUE;
}
void net_chat_release(void)
{
    if (!s_events) return;
    xEventGroupClearBits(s_events, BIT_CHAT_IDLE);
    post(CMD_CHAT_OFF);
}
bool net_chat_idle(void)
{
    return s_events && (xEventGroupGetBits(s_events) & BIT_CHAT_IDLE) != 0;
}
bool net_chat_ready(void)
{
    if (!s_events) return false;
    return (xEventGroupGetBits(s_events) & BIT_CHAT_READY) != 0;
}
