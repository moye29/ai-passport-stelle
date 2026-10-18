#include "app_store.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t durable, pending;
static bool present;
static int fail_read, fail_set, fail_commit, fail_init;
esp_err_t nvs_flash_init(void) { return fail_init; }
esp_err_t nvs_open(const char *space, int mode, nvs_handle_t *h)
{ assert(!strcmp(space,"stelle") && mode==NVS_READWRITE); *h=1; return ESP_OK; }
esp_err_t nvs_get_u32(nvs_handle_t h,const char *key,uint32_t *out)
{ assert(h==1 && !strcmp(key,"path_energy")); if(fail_read)return 2;
  if(!present)return ESP_ERR_NVS_NOT_FOUND;
  *out=durable;
  return ESP_OK; }
esp_err_t nvs_set_u32(nvs_handle_t h,const char *key,uint32_t value)
{ assert(h==1 && !strcmp(key,"path_energy")); pending=value; return fail_set; }
esp_err_t nvs_commit(nvs_handle_t h)
{ assert(h==1); if(fail_commit)return 2; durable=pending; present=true; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h,const char *k,void *p,size_t *n)
{ (void)h;(void)k;(void)p;(void)n; return ESP_ERR_NVS_NOT_FOUND; }
esp_err_t nvs_set_blob(nvs_handle_t h,const char *k,const void *p,size_t n)
{ (void)h;(void)k;(void)p;(void)n; return ESP_OK; }
int main(void)
{
    uint32_t value=55;
    assert(app_store_open() && app_store_load_energy(&value) && value==0);
    assert(app_store_save_energy(123456));
    assert(app_store_open() && app_store_load_energy(&value) && value==123456);
    fail_commit=1; assert(!app_store_save_energy(0)); fail_commit=0;
    assert(app_store_load_energy(&value) && value==123456);
    fail_set=1; assert(!app_store_save_energy(99)); fail_set=0;
    assert(app_store_load_energy(&value) && value==123456);
    fail_read=1; assert(!app_store_load_energy(&value)); fail_read=0;
    assert(app_store_save_energy(0));
    assert(app_store_open() && app_store_load_energy(&value) && value==0);
    fail_init=1; assert(!app_store_open() && !app_store_save_energy(1));
    puts("Battle NVS: first boot, restart, clear, failed reads/writes/commits: PASS");
}
