// main/net.h - Wi-Fi setup portal and network time (ESP-IDF platform layer).
//
// All radio work runs in one network task; these calls only queue commands or
// copy the status, so they are safe from the platform loop and never block.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "net_status.h"

bool net_init(void);                      // after nvs_flash_init(); false: Wi-Fi unavailable
void net_get_status(net_status_t *out);   // consistent snapshot
void net_start_portal(void);              // hotspot + captive setup page
void net_stop_portal(void);               // back to idle (keeps saved credentials)
void net_sync_time(void);                 // join the saved network, fetch time, radio off
// New local time from the network since the last call: seconds since midnight
// and the local day number (days since 1970-01-01, see focus_log.h).
bool net_take_time(uint32_t *seconds_of_day, int32_t *day);
// A Chat lease keeps the station online. The network task remains its owner.
bool net_chat_acquire(void); // queues connection; false if command cannot be queued
void net_chat_release(void);
bool net_chat_ready(void);
bool net_chat_idle(void); // true only after the network worker releases the Chat lease
