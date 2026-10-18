// Official FoloToy XiaoZhi wire contract adapted to ESP-IDF 5.5.3.
// See docs/stelle-chat.md for the pinned reference, scope and provenance.
// No OTA writes, wake-word engine, remote tools, default XiaoZhi UI or sleep policy.
#include "chat_service.h"
#include "chat_wire.h"
#include "chat_cache.h"
#include "chat_codec_memory.h"
#include "chat_tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "bsp_audio.h"
#include "net.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include "mqtt_client.h"
#include "mbedtls/aes.h"
#include "nvs.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"
#define MSG_BYTES 2048U
#define PACKET_BYTES 1500U
#define PCM_SAMPLES 960U // 60 ms, 16 kHz, mono
#define EV_START BIT0
#define EV_CANCEL BIT1
#define EV_STOP BIT2
#define EV_MQTT_UP BIT3
#define EV_MQTT_DOWN BIT4
#define EV_OVERFLOW BIT5
#define EV_RELEASE BIT6

typedef struct { size_t size; char data[MSG_BYTES]; } message_t;
typedef struct {
    char endpoint[160], client[192], user[256], pass[256], publish[160], subscribe[160];
} broker_t;
static EventGroupHandle_t s_events;
static QueueHandle_t s_messages, s_texts;
static SemaphoreHandle_t s_lock, s_audio;
static chat_service_status_t s_status;
static esp_mqtt_client_handle_t s_mqtt;
static int s_udp = -1;
static char s_uuid[37], s_mac[18], s_session[128];
static broker_t s_broker;
static uint8_t s_nonce[16], s_key[16];
static uint32_t s_tx, s_rx;
static bool s_hello, s_tts_end;
static bool s_server_speaking;
static uint32_t s_generation, s_worker_generation;
static int64_t s_last_audio;
static void *s_encoder, *s_decoder;
static message_t s_fragment; // MQTT event task only; not on its stack
static uint8_t s_packet[PACKET_BYTES], s_plain[PACKET_BYTES];
static int16_t s_pcm[PCM_SAMPLES];
static const char *TAG = "stelle_chat";
static const char *s_failure_hint;
static chat_cache_t s_cache;
static bool s_foreground;
static volatile size_t s_failed_alloc_size;
static volatile uint32_t s_failed_alloc_caps;
static void allocation_failed(size_t size, uint32_t caps, const char *function)
{
    (void)function;
    // Do not allocate, block or log inside the allocator failure callback.
    s_failed_alloc_size=size;
    s_failed_alloc_caps=caps;
}

static void memory_log(const char *stage)
{
    const chat_codec_pool_t *pool=chat_codec_memory_stats();
    ESP_LOGI(TAG, "%s free=%u largest=%u stack=%u codec_used=%u codec_peak=%u codec_failed=%u", stage,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)uxTaskGetStackHighWaterMark(NULL),
             (unsigned)pool->used,(unsigned)pool->peak,(unsigned)pool->failed_request);
}

static uint32_t ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static void status_set(chat_phase_t phase, const char *hint)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_generation == s_worker_generation) {
        s_status.phase = phase;
        snprintf(s_status.hint, sizeof(s_status.hint), "%s", hint ? hint : "");
        if (phase != CHAT_BINDING) s_status.code[0] = 0;
    }
    xSemaphoreGive(s_lock);
}
void chat_service_get_status(chat_service_status_t *out)
{
    if (!s_lock) { memset(out, 0, sizeof(*out)); out->phase = CHAT_ERROR; return; }
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_status; xSemaphoreGive(s_lock);
}
static bool cancelled(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool stale = s_generation != s_worker_generation;
    xSemaphoreGive(s_lock);
    return stale || (xEventGroupGetBits(s_events) & (EV_CANCEL|EV_RELEASE)) != 0;
}
static bool foreground(void)
{
    xSemaphoreTake(s_lock,portMAX_DELAY); bool value=s_foreground; xSemaphoreGive(s_lock);
    return value;
}
static bool transport_up(void)
{
    EventBits_t bits=xEventGroupGetBits(s_events);
    return s_mqtt && (bits&EV_MQTT_UP) && !(bits&(EV_MQTT_DOWN|EV_OVERFLOW));
}
static void drain_messages(void)
{
    char *payload;
    while (xQueueReceive(s_messages,&payload,0)==pdTRUE) free(payload);
}
static const char *str(const cJSON *obj, const char *name)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, name);
    return cJSON_IsString(v) ? v->valuestring : "";
}
static bool field(const cJSON *obj, const char *key, char *out, size_t size, bool required)
{
    const char *v = str(obj, key);
    if (strlen(v) >= size || (required && !*v)) return false;
    memcpy(out, v, strlen(v)+1); return true;
}
static bool identity(void)
{
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) return false;
    snprintf(s_mac, sizeof(s_mac), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvs_handle_t nvs;
    if (nvs_open("board", NVS_READWRITE, &nvs) != ESP_OK) return false;
    size_t len = sizeof(s_uuid);
    if (nvs_get_str(nvs, "uuid", s_uuid, &len) != ESP_OK || strlen(s_uuid) != 36) {
        uint8_t u[16]; esp_fill_random(u, sizeof(u)); u[6]=(u[6]&15)|64; u[8]=(u[8]&63)|128;
        snprintf(s_uuid,sizeof(s_uuid),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 u[0],u[1],u[2],u[3],u[4],u[5],u[6],u[7],u[8],u[9],u[10],u[11],u[12],u[13],u[14],u[15]);
        if (nvs_set_str(nvs,"uuid",s_uuid) != ESP_OK || nvs_commit(nvs) != ESP_OK) { nvs_close(nvs); return false; }
    }
    nvs_close(nvs); return true;
}
static char *device_info(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root,"version",2);
    cJSON_AddStringToObject(root,"language","zh-CN");
    cJSON_AddNumberToObject(root,"flash_size",8*1024*1024);
    cJSON_AddNumberToObject(root,"psram_size",0);
    cJSON_AddStringToObject(root,"mac_address",s_mac);
    cJSON_AddStringToObject(root,"uuid",s_uuid);
    cJSON_AddStringToObject(root,"chip_model_name","esp32c3");
    cJSON *application=cJSON_AddObjectToObject(root,"application");
    cJSON_AddStringToObject(application,"name",app->project_name);
    cJSON_AddStringToObject(application,"version",app->version);
    cJSON_AddStringToObject(application,"idf_version",app->idf_ver);
    cJSON *board=cJSON_AddObjectToObject(root,"board");
    cJSON_AddStringToObject(board,"type","ai-passport");
    cJSON_AddStringToObject(board,"name","stelle-companion");
    cJSON_AddStringToObject(board,"manufacturer","folotoy");
    cJSON_AddStringToObject(board,"mac",s_mac);
    char *body=cJSON_PrintUnformatted(root); cJSON_Delete(root); return body;
}
// Bounded streaming response; never log credentials, identity, activation codes or text.
static cJSON *http_post(const char *url, const char *body, int *status)
{
    esp_http_client_config_t cfg={.url=url,.timeout_ms=8000,.crt_bundle_attach=chat_tls_attach,
                                 .buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);
    *status=0;
    if (!h) return NULL;
    esp_http_client_set_method(h,HTTP_METHOD_POST);
    esp_http_client_set_header(h,"Content-Type","application/json");
    esp_http_client_set_header(h,"Activation-Version","1");
    esp_http_client_set_header(h,"Device-Id",s_mac);
    esp_http_client_set_header(h,"Client-Id",s_uuid);
    esp_http_client_set_header(h,"User-Agent","ai-passport/stelle-1.0");
    esp_http_client_set_header(h,"Accept-Language","zh-CN");
    char *response=malloc(8192);
    cJSON *root=NULL;
    if (response && esp_http_client_open(h,(int)strlen(body)) == ESP_OK &&
        esp_http_client_write(h,body,(int)strlen(body)) == (int)strlen(body) &&
        esp_http_client_fetch_headers(h) >= 0) {
        *status=esp_http_client_get_status_code(h);
        size_t used=0;
        int n=0;
        while (!cancelled() && used<8191 && (n=esp_http_client_read(h,response+used,8191-used))>0) used+=(size_t)n;
        response[used]=0;
        if (n>=0 && esp_http_client_is_complete_data_received(h)) root=cJSON_Parse(response);
    }
    if (response) { memset(response,0,8192); free(response); }
    esp_http_client_close(h); esp_http_client_cleanup(h); return root;
}
static bool provision(void)
{
    char *body=device_info();
    if (!body) return false;
    bool ok=false;
    uint32_t started=ms();
    while (!cancelled() && ms()-started<180000U) {
        int code;
        cJSON *root=http_post(OTA_URL,body,&code);
        if (!root || code != 200) { cJSON_Delete(root); break; }
        const cJSON *activation=cJSON_GetObjectItem(root,"activation");
        const char *bind_code=str(activation,"code");
        if (*bind_code) {
            status_set(CHAT_BINDING,"Bind at xiaozhi.me");
            xSemaphoreTake(s_lock,portMAX_DELAY);
            if (s_generation == s_worker_generation) snprintf(s_status.code,sizeof(s_status.code),"%.15s",bind_code);
            xSemaphoreGive(s_lock);
            bool challenge=*str(activation,"challenge") != 0;
            cJSON_Delete(root);
            for (unsigned i=0;i<20 && !cancelled();i++) vTaskDelay(pdMS_TO_TICKS(100));
            if (challenge && !cancelled()) {
                cJSON *r=http_post(OTA_URL "activate","{}",&code); cJSON_Delete(r);
            }
            continue;
        }
        const cJSON *mqtt=cJSON_GetObjectItem(root,"mqtt");
        ok=cJSON_IsObject(mqtt) && field(mqtt,"endpoint",s_broker.endpoint,sizeof(s_broker.endpoint),true) &&
           field(mqtt,"client_id",s_broker.client,sizeof(s_broker.client),true) &&
           field(mqtt,"username",s_broker.user,sizeof(s_broker.user),true) &&
           field(mqtt,"password",s_broker.pass,sizeof(s_broker.pass),true) &&
           field(mqtt,"publish_topic",s_broker.publish,sizeof(s_broker.publish),true) &&
           field(mqtt,"subscribe_topic",s_broker.subscribe,sizeof(s_broker.subscribe),false);
        cJSON_Delete(root); break;
    }
    free(body); return ok;
}
static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    esp_mqtt_event_handle_t e=data;
    if (id==MQTT_EVENT_CONNECTED) {
        if (*s_broker.subscribe && strcmp(s_broker.subscribe,"null")) esp_mqtt_client_subscribe(e->client,s_broker.subscribe,0);
        xEventGroupSetBits(s_events,EV_MQTT_UP);
    } else if (id==MQTT_EVENT_DISCONNECTED || id==MQTT_EVENT_ERROR) {
        if (id==MQTT_EVENT_ERROR) {
            memory_log("MQTT transport failed");
            ESP_LOGE(TAG,"TLS error=%d failed_alloc=%u caps=0x%lx",
                     e->error_handle ? e->error_handle->esp_tls_stack_err : 0,
                     (unsigned)s_failed_alloc_size,(unsigned long)s_failed_alloc_caps);
        }
        xEventGroupSetBits(s_events,EV_MQTT_DOWN);
    }
    else if (id==MQTT_EVENT_DATA) {
        if (e->current_data_offset==0) s_fragment.size=0;
        if (e->total_data_len<=0 || e->total_data_len>=(int)MSG_BYTES || e->data_len<0 ||
            e->current_data_offset != (int)s_fragment.size ||
            s_fragment.size+(size_t)e->data_len >= MSG_BYTES) { xEventGroupSetBits(s_events,EV_OVERFLOW); return; }
        memcpy(s_fragment.data+s_fragment.size,e->data,e->data_len); s_fragment.size+=e->data_len;
        if (s_fragment.size==(size_t)e->total_data_len) {
            s_fragment.data[s_fragment.size]=0;
            // Queue only the received bytes, rather than four permanently
            // reserved 2KB payloads. Capacity and maximum message size stay bounded.
            char *payload=malloc(s_fragment.size+1);
            if (payload) memcpy(payload,s_fragment.data,s_fragment.size+1);
            if (!payload || xQueueSend(s_messages,&payload,0)!=pdTRUE) {
                free(payload);
                xEventGroupSetBits(s_events,EV_OVERFLOW);
            }
        }
    }
}
static bool send_json(cJSON *root)
{
    char *text=cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if (!text) return false;
    bool ok=s_mqtt && esp_mqtt_client_publish(s_mqtt,s_broker.publish,text,0,0,0)>=0;
    free(text); return ok;
}
static bool control(const char *type, const char *state)
{
    cJSON *r=cJSON_CreateObject(); cJSON_AddStringToObject(r,"session_id",s_session);
    cJSON_AddStringToObject(r,"type",type);
    if (state) cJSON_AddStringToObject(r,"state",state);
    if (!strcmp(type,"listen") && state && !strcmp(state,"start")) cJSON_AddStringToObject(r,"mode","manual");
    return send_json(r);
}
static bool audio_crypt(const uint8_t *in,size_t n,const uint8_t *iv,uint8_t *out)
{
    mbedtls_aes_context aes; mbedtls_aes_init(&aes);
    uint8_t nonce[16], stream[16]={0}; memcpy(nonce,iv,16); size_t off=0;
    bool ok=mbedtls_aes_setkey_enc(&aes,s_key,128)==0 && mbedtls_aes_crypt_ctr(&aes,n,&off,nonce,stream,in,out)==0;
    mbedtls_aes_free(&aes); return ok;
}
static bool server_hello(const cJSON *root)
{
    if (strcmp(str(root,"transport"),"udp") || !field(root,"session_id",s_session,sizeof(s_session),true)) return false;
    const cJSON *params=cJSON_GetObjectItem(root,"audio_params"), *udp=cJSON_GetObjectItem(root,"udp");
    cJSON *duration=cJSON_GetObjectItem(params,"frame_duration"), *port=cJSON_GetObjectItem(udp,"port");
    if ((cJSON_IsNumber(duration) && duration->valueint!=60) || !cJSON_IsNumber(port) || port->valueint<1 || port->valueint>65535 ||
        !chat_wire_hex16(str(udp,"key"),s_key) || !chat_wire_hex16(str(udp,"nonce"),s_nonce)) return false;
    struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_DGRAM}, *addr=NULL;
    char service[8]; snprintf(service,sizeof(service),"%d",port->valueint);
    if (getaddrinfo(str(udp,"server"),service,&hints,&addr)!=0 || !addr) return false;
    s_udp=socket(addr->ai_family,SOCK_DGRAM,IPPROTO_UDP);
    bool ok=s_udp>=0 && connect(s_udp,addr->ai_addr,addr->ai_addrlen)==0;
    freeaddrinfo(addr);
    if (!ok) return false;
    fcntl(s_udp,F_SETFL,O_NONBLOCK);
    s_tx=s_rx=0; return true;
}
static bool incoming(const char *payload)
{
    cJSON *r=cJSON_Parse(payload);
    if (!r) return false;
    const char *type=str(r,"type");
    bool ok=true;
    const char *session=str(r,"session_id");
    if (!strcmp(type,"hello")) {
        if (!s_hello) {
            s_hello=server_hello(r); ok=s_hello;
        }
    } else if (!strcmp(type,"alert")) ok=false;
    else if (!chat_cache_accept_session(s_hello,s_session,session)) { /* Late previous-round data. */ }
    else if (!strcmp(type,"stt") || (!strcmp(type,"tts") && !strcmp(str(r,"state"),"sentence_start"))) {
        chat_entry_t entry={.assistant=!strcmp(type,"tts")};
        const char *text=str(r,"text");
        if (*text) {
            chat_utf8_copy(entry.text,sizeof(entry.text),text);
            if (xQueueSend(s_texts,&entry,pdMS_TO_TICKS(50))!=pdTRUE) ok=false;
        }
    } else if (!strcmp(type,"tts")) {
        if (!strcmp(str(r,"state"),"start")) { s_server_speaking=true; s_tts_end=false; status_set(CHAT_WAITING,"Waiting for voice..."); }
        else if (!strcmp(str(r,"state"),"stop")) { s_tts_end=true; s_last_audio=esp_timer_get_time(); }
    } else if (!strcmp(type,"goodbye")) {
        const char *session=str(r,"session_id");
        if (s_hello && (!*session || !strcmp(session,s_session))) ok=false;
    }
    cJSON_Delete(r); return ok;
}
static bool pump_messages(void)
{
    EventBits_t bits=xEventGroupGetBits(s_events);
    if (bits & (EV_MQTT_DOWN|EV_OVERFLOW)) return false;
    char *payload;
    while (xQueueReceive(s_messages,&payload,0)==pdTRUE) {
        bool ok=incoming(payload);
        free(payload);
        if (!ok) return false;
    }
    return true;
}
static bool open_channel(void)
{
    uint32_t start;
    if (!s_mqtt) {
    char uri[192];
    // Official OTA endpoint uses host[:port]; enforce TLS for broker credentials.
    if (strchr(s_broker.endpoint,'/') || snprintf(uri,sizeof(uri),"mqtts://%s",s_broker.endpoint)>=(int)sizeof(uri)) return false;
    esp_mqtt_client_config_t cfg={
        .broker.address.uri=uri,.broker.verification.crt_bundle_attach=chat_tls_attach,
        .credentials.client_id=s_broker.client,.credentials.username=s_broker.user,
        .credentials.authentication.password=s_broker.pass,
        .session.keepalive=240,.network.disable_auto_reconnect=true,.network.timeout_ms=8000,
        .buffer.size=1024,.buffer.out_size=1024,.task.stack_size=4096};
    s_mqtt=esp_mqtt_client_init(&cfg);
    if (!s_mqtt || esp_mqtt_client_register_event(s_mqtt,ESP_EVENT_ANY_ID,mqtt_event,NULL)!=ESP_OK ||
        esp_mqtt_client_start(s_mqtt)!=ESP_OK) return false;
    start=ms();
    while (!cancelled() && !(xEventGroupGetBits(s_events)&EV_MQTT_UP)) {
        if (ms()-start>10000 || !pump_messages()) return false;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    if (cancelled()) return false;
    }
    cJSON *root=cJSON_CreateObject();
    cJSON_AddStringToObject(root,"type","hello"); cJSON_AddNumberToObject(root,"version",3);
    cJSON_AddStringToObject(root,"transport","udp");
    cJSON *params=cJSON_AddObjectToObject(root,"audio_params");
    cJSON_AddStringToObject(params,"format","opus"); cJSON_AddNumberToObject(params,"sample_rate",16000);
    cJSON_AddNumberToObject(params,"channels",1); cJSON_AddNumberToObject(params,"frame_duration",60);
    if (!send_json(root)) return false;
    start=ms();
    while (!s_hello && !cancelled()) {
        if (ms()-start>10000 || !pump_messages()) return false;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return s_hello && !cancelled();
}
static bool encoder_open(void)
{
    if (s_encoder) return true;
    if (!chat_codec_memory_reserve()) {
        memory_log("codec reservation failed");
        s_failure_hint="Audio unavailable / OK: retry";
        return false;
    }
    esp_opus_enc_config_t enc={.sample_rate=16000,.channel=1,.bits_per_sample=16,.bitrate=24000,
        .frame_duration=ESP_OPUS_ENC_FRAME_DURATION_60_MS,.application_mode=ESP_OPUS_ENC_APPLICATION_VOIP,
        .complexity=0,.enable_fec=false,.enable_dtx=false,.enable_vbr=true};
    memory_log("encoder opening");
    esp_audio_err_t err=esp_opus_enc_open(&enc,sizeof(enc),&s_encoder);
    if (err!=ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG,"encoder open failed code=%d",(int)err);
        s_failure_hint="Audio unavailable / OK: retry";
        return false;
    }
    memory_log("encoder ready");
    return true;
}
static bool decoder_open(void)
{
    // Manual conversation never captures and plays simultaneously. Release the
    // larger encoder before allocating playback state on the no-PSRAM C3.
    if (s_encoder) { esp_opus_enc_close(s_encoder); s_encoder=NULL; }
    esp_opus_dec_cfg_t dec={.sample_rate=16000,.channel=1,.frame_duration=ESP_OPUS_DEC_FRAME_DURATION_60_MS,.self_delimited=false};
    memory_log("decoder opening");
    esp_audio_err_t err=esp_opus_dec_open(&dec,sizeof(dec),&s_decoder);
    if (err!=ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG,"decoder open failed code=%d",(int)err);
        s_failure_hint="Audio unavailable / OK: retry";
        return false;
    }
    memory_log("decoder ready");
    return true;
}
static bool record_frame(void)
{
    if (bsp_audio_read(s_pcm,sizeof(s_pcm))!=ESP_OK) return false;
    if (cancelled() || (xEventGroupGetBits(s_events)&EV_STOP)) return true;
    esp_audio_enc_in_frame_t in={.buffer=(uint8_t *)s_pcm,.len=sizeof(s_pcm)};
    esp_audio_enc_out_frame_t out={.buffer=s_plain,.len=sizeof(s_plain)-16};
    if (esp_opus_enc_process(s_encoder,&in,&out)!=ESP_AUDIO_ERR_OK || out.encoded_bytes>sizeof(s_packet)-16) return false;
    chat_wire_header(s_packet,s_nonce,(uint16_t)out.encoded_bytes,++s_tx);
    return audio_crypt(s_plain,out.encoded_bytes,s_packet,s_packet+16) &&
           send(s_udp,s_packet,out.encoded_bytes+16,0)==(int)out.encoded_bytes+16;
}
static int play_frame(void)
{
    int n=recv(s_udp,s_packet,sizeof(s_packet),MSG_DONTWAIT);
    if (n<0) return (errno==EAGAIN || errno==EWOULDBLOCK) ? 0 : -1;
    uint32_t seq;
    if (!chat_wire_accept(s_packet,(size_t)n,s_rx,&seq)) return 0;
    size_t len=(size_t)n-16;
    if (!audio_crypt(s_packet+16,len,s_packet,s_plain)) return 0;
    s_rx=seq;
    esp_audio_dec_in_raw_t raw={.buffer=s_plain,.len=len};
    esp_audio_dec_out_frame_t frame={.buffer=(uint8_t *)s_pcm,.len=sizeof(s_pcm)};
    esp_audio_dec_info_t info={0};
    if (esp_opus_dec_decode(s_decoder,&raw,&frame,&info)!=ESP_AUDIO_ERR_OK) return -1;
    if (cancelled()) return 0;
    status_set(CHAT_PLAYING,"OK: interrupt");
    if (bsp_audio_write(s_pcm,frame.decoded_size)!=ESP_OK) return -1;
    s_last_audio=esp_timer_get_time();
    return 1;
}
static void close_audio(void)
{
    if (s_mqtt && s_hello) control("goodbye",NULL);
    if (s_udp>=0) { close(s_udp); s_udp=-1; }
    if (s_encoder) { esp_opus_enc_close(s_encoder); s_encoder=NULL; }
    if (s_decoder) { esp_opus_dec_close(s_decoder); s_decoder=NULL; }
    memset(s_key,0,sizeof(s_key)); memset(s_nonce,0,sizeof(s_nonce));
    s_session[0]=0; s_hello=s_tts_end=s_server_speaking=false;
    drain_messages();
}
static void stop_mqtt(void)
{
    if (s_mqtt) { esp_mqtt_client_stop(s_mqtt); esp_mqtt_client_destroy(s_mqtt); s_mqtt=NULL; }
    drain_messages();
    xEventGroupClearBits(s_events,EV_MQTT_UP|EV_MQTT_DOWN|EV_OVERFLOW);
}
static void disconnect(void)
{
    close_audio();stop_mqtt();
    if (!chat_codec_memory_release()) ESP_LOGE(TAG,"codec reservation still in use at disconnect");
    memset(&s_broker,0,sizeof(s_broker));chat_cache_clear(&s_cache);
    net_chat_release();
}
static bool connect_cold(void)
{
    disconnect();
    uint32_t released=ms();
    while (!net_chat_idle() && !cancelled() && ms()-released<65000U) vTaskDelay(pdMS_TO_TICKS(20));
    if (!net_chat_idle() || cancelled()) return false;
    // Preserve the proven allocation order: reserve codec before Wi-Fi/TLS.
    if (!identity() || !encoder_open() || !net_chat_acquire()) return false;
    uint32_t started=ms();
    while (!net_chat_ready() && !cancelled() && ms()-started<65000) vTaskDelay(pdMS_TO_TICKS(50));
    return net_chat_ready() && !cancelled() && provision() && !cancelled() && open_channel();
}
static void task(void *arg)
{
    (void)arg;
    chat_codec_memory_bind();
    for (;;) {
        EventBits_t bits=xEventGroupWaitBits(s_events,EV_START|EV_CANCEL|EV_RELEASE,pdTRUE,pdFALSE,pdMS_TO_TICKS(500));
        if (!(bits&EV_START)) {
            if (s_cache.valid && !chat_cache_reusable(&s_cache,ms(),foreground(),net_chat_ready(),transport_up())) disconnect();
            else drain_messages();
            continue;
        }
        xSemaphoreTake(s_lock,portMAX_DELAY); s_worker_generation=s_generation; xSemaphoreGive(s_lock);
        status_set(CHAT_CONNECTING,"Connecting to XiaoZhi...");
        s_failure_hint="Connection failed / OK: retry";
        s_failed_alloc_size=0;
        s_failed_alloc_caps=0;
        uint32_t connect_started=ms(),start=connect_started;
        bool reused=chat_cache_reusable(&s_cache,start,foreground(),net_chat_ready(),transport_up());
        bool audio_locked=false,ok;
        if (reused) {
            drain_messages();
            ok=encoder_open() && open_channel();
            if (!ok && !cancelled()) {
                // Expired server sessions or heap pressure fall back once to
                // the proven full setup, without requiring another button press.
                s_failure_hint="Connection failed / OK: retry";
                reused=false;ok=connect_cold();
            }
        } else ok=connect_cold();
        if (ok && !cancelled()) { audio_locked=xSemaphoreTake(s_audio,pdMS_TO_TICKS(1500))==pdTRUE; ok=audio_locked; }
        if (ok && !cancelled()) ok=control("listen","start");
        if (ok && !cancelled()) {
            ESP_LOGI(TAG,"voice ready in %u ms reused=%d",(unsigned)(ms()-connect_started),reused);
            status_set(CHAT_RECORDING,"OK: finish recording");
        }
        bool recording=true;
        start=ms(); s_last_audio=esp_timer_get_time();
        while (ok && !cancelled()) {
            if (!net_chat_ready() || !pump_messages()) { ok=false; break; }
            if (recording && ((xEventGroupGetBits(s_events)&EV_STOP) || s_server_speaking)) {
                recording=false;
                if (!s_server_speaking) ok=control("listen","stop");
                if (ok) ok=decoder_open();
                status_set(CHAT_WAITING,"Waiting for reply..."); start=ms();
            }
            if (recording) ok=record_frame();
            else {
                int played=play_frame();
                if (played<0) ok=false;
                if (!played) {
                    if (esp_timer_get_time()-s_last_audio>120000) status_set(CHAT_WAITING,"Waiting for reply...");
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
                if (s_tts_end && esp_timer_get_time()-s_last_audio>240000) break;
            }
            if (ms()-start>120000U) { ok=false; break; }
        }
        bool was_cancelled=cancelled();
        if (was_cancelled && s_hello) control("abort",NULL);
        if (audio_locked) xSemaphoreGive(s_audio);
        close_audio();
        if ((ok || was_cancelled) && foreground() && net_chat_ready() && transport_up()) {
            chat_cache_touch(&s_cache,ms());
            // Reserve the next encoder while the just-closed decoder's region
            // is free; no PCM is captured until the next explicit start.
            if (!encoder_open()) disconnect();
        } else disconnect();
        status_set(ok || was_cancelled ? CHAT_OFF : CHAT_ERROR,ok || was_cancelled ? "OK: start / Hold: history" : s_failure_hint);
    }
}
void chat_service_set_foreground(bool active)
{
    if (!s_events || !s_lock) return;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    bool changed=s_foreground!=active;s_foreground=active;
    xSemaphoreGive(s_lock);
    if (changed) {
        if (active) xEventGroupClearBits(s_events,EV_RELEASE);
        else xEventGroupSetBits(s_events,EV_RELEASE);
    }
}
bool chat_service_init(SemaphoreHandle_t audio_mutex)
{
    heap_caps_register_failed_alloc_callback(allocation_failed);
    s_audio=audio_mutex;
    s_lock=xSemaphoreCreateMutex(); s_events=xEventGroupCreate();
    s_messages=xQueueCreate(4,sizeof(char *)); s_texts=xQueueCreate(4,sizeof(chat_entry_t));
    if (!s_audio || !s_lock || !s_events || !s_messages || !s_texts) return false;
    // Matched crash: SILK frame alone reserves 9472 bytes; the observed
    // task stack already reached 14240 bytes before its child calls. Budget
    // 27KB: the successful 24KB live test left only 552 bytes, so add 3KB
    // margin, funded by two fewer static Wi-Fi RX buffers (about 3.2KB).
    return xTaskCreate(task,"xiaozhi",27648,NULL,4,NULL)==pdPASS;
}
void chat_service_command(chat_cmd_t cmd)
{
    if (!s_events) return;
    if (cmd==CHAT_CMD_START) {
        xSemaphoreTake(s_lock,portMAX_DELAY);
        s_generation++;
        s_status.phase=CHAT_CONNECTING;
        s_status.code[0]=0;
        snprintf(s_status.hint,sizeof(s_status.hint),"Connecting to XiaoZhi...");
        xSemaphoreGive(s_lock);
        xEventGroupClearBits(s_events,EV_CANCEL|EV_STOP);
        xEventGroupSetBits(s_events,EV_START);
    }
    else if (cmd==CHAT_CMD_STOP_RECORDING) {
        // A pending 60 ms I2S read must not put the UI back into recording.
        xSemaphoreTake(s_lock,portMAX_DELAY);
        if (s_status.phase==CHAT_RECORDING) {
            s_status.phase=CHAT_WAITING;
            snprintf(s_status.hint,sizeof(s_status.hint),"Waiting for reply...");
        }
        xSemaphoreGive(s_lock);
        xEventGroupSetBits(s_events,EV_STOP);
    }
    else if (cmd==CHAT_CMD_CANCEL) {
        xSemaphoreTake(s_lock,portMAX_DELAY); s_generation++; s_status.phase=CHAT_OFF; s_status.code[0]=0; xSemaphoreGive(s_lock);
        xEventGroupSetBits(s_events,EV_CANCEL);
    }
}
bool chat_service_take_text(chat_entry_t *out) { return s_texts && xQueueReceive(s_texts,out,0)==pdTRUE; }
