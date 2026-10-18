// main/focus_log.h - focused time per calendar day (pure C, persisted as one blob).
//
// A ring of FOCUS_LOG_DAYS slots indexed by local day number (days since
// 1970-01-01 in the device's time zone). Each slot remembers which day it
// holds, so a stale slot from a year ago reads as empty. Time is added while
// a session runs, so a stopped or reset session keeps what was focused.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FOCUS_LOG_DAYS 366
#define FOCUS_LOG_VERSION 1

typedef struct {
    int32_t day;        // local day number this slot holds
    uint32_t secs;      // focused seconds that day
    uint16_t sessions;  // sessions started that day
    uint16_t completed; // was zero-filled reserved space in v1; blob size unchanged
} focus_day_t;

typedef struct {
    uint16_t version;
    uint16_t reserved;
    focus_day_t slot[FOCUS_LOG_DAYS];
} focus_log_t;

void focus_log_init(focus_log_t *log);
bool focus_log_valid(const focus_log_t *log);
void focus_log_add(focus_log_t *log, int32_t day, uint32_t secs, uint16_t sessions);
void focus_log_add_completions(focus_log_t *log, int32_t day, uint16_t count);
focus_day_t focus_log_get(const focus_log_t *log, int32_t day);  // zeros when nothing is stored

// Civil calendar helpers (proleptic Gregorian, local day numbers).
int32_t date_to_day(int year, int month, int mday);
void date_from_day(int32_t day, int *year, int *month, int *mday);
int date_weekday_mon0(int32_t day);   // 0 = Monday ... 6 = Sunday
int date_days_in_month(int year, int month);
