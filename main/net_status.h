// main/net_status.h - network state shared by the platform task and the view.
// Plain C (no ESP-IDF types) so the view, copy and native preview can use it.
#pragma once

#include <stdbool.h>

typedef enum {
    NET_PHASE_IDLE = 0,       // radio off
    NET_PHASE_STARTING,       // scanning / bringing the hotspot up
    NET_PHASE_PORTAL,         // hotspot + setup page are up, waiting for the phone
    NET_PHASE_JOINING,        // trying the network the phone sent
    NET_PHASE_JOIN_FAILED,    // hotspot still up; `message` says why
    NET_PHASE_CONNECTING,     // reconnecting with saved credentials
    NET_PHASE_SYNCING,        // connected, waiting for network time
    NET_PHASE_SYNCED,         // time set; radio turned off again
    NET_PHASE_OFFLINE,        // saved network unreachable or no time answer
} net_phase_t;

typedef struct {
    net_phase_t phase;
    bool has_credentials;     // a home network is saved
    bool phone_joined;        // a phone is associated with the hotspot
    char ap_ssid[24];         // hotspot name, e.g. "Stelle-3F2A"
    char ap_pass[12];         // random 8-digit hotspot password
    char ap_qr[80];           // "WIFI:T:WPA;S:...;P:...;;" for the phone camera
    char sta_ssid[33];        // home network being joined / saved
    char message[64];         // last join error, shown on the setup page
} net_status_t;
