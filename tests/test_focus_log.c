#include "focus_log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_dates(void)
{
    assert(date_to_day(1970, 1, 1) == 0);
    assert(date_to_day(2026, 10, 16) == 20742);
    assert(date_to_day(2023, 4, 26) == 19473);
    int y, m, d;
    date_from_day(20742, &y, &m, &d);
    assert(y == 2026 && m == 10 && d == 16);
    date_from_day(date_to_day(2024, 2, 29), &y, &m, &d);
    assert(y == 2024 && m == 2 && d == 29);
    date_from_day(-1, &y, &m, &d);
    assert(y == 1969 && m == 12 && d == 31);
    assert(date_weekday_mon0(0) == 3);       // 1970-01-01 was a Thursday
    assert(date_weekday_mon0(20742) == 4);   // 2026-10-16 is a Friday
    assert(date_days_in_month(2024, 2) == 29 && date_days_in_month(2026, 2) == 28);
    assert(date_days_in_month(2100, 2) == 28 && date_days_in_month(2000, 2) == 29);
    assert(date_days_in_month(2026, 12) == 31 && date_days_in_month(2026, 4) == 30);
}

static void test_log_ring(void)
{
    static focus_log_t log;
    focus_log_init(&log);
    assert(focus_log_valid(&log));
    assert(focus_log_get(&log, 20742).secs == 0);
    focus_log_add(&log, 20742, 600, 1);
    focus_log_add(&log, 20742, 120, 1);
    focus_day_t d = focus_log_get(&log, 20742);
    assert(d.day == 20742 && d.secs == 720 && d.sessions == 2);
    // The same slot a year later belongs to the new day; the old one reads empty.
    focus_log_add(&log, 20742 + FOCUS_LOG_DAYS, 60, 1);
    assert(focus_log_get(&log, 20742).secs == 0);
    assert(focus_log_get(&log, 20742 + FOCUS_LOG_DAYS).secs == 60);
    focus_log_add(&log, 20000, 90000, 0);  // capped at one day
    assert(focus_log_get(&log, 20000).secs == 86400);
    assert(focus_log_valid(&log));
    log.slot[5].day = 6;  // a slot holding a day that maps elsewhere: corrupt
    assert(!focus_log_valid(&log));
    focus_log_init(&log);
    log.version = 99;
    assert(!focus_log_valid(&log));
}

static void test_completion_storage_compatibility(void)
{
    static focus_log_t log;
    assert(sizeof(focus_day_t)==12 && sizeof(focus_log_t)==4396);
    focus_log_init(&log);
    focus_log_add(&log,20742,600,1); // old zero-filled reserved field reads as zero completions
    assert(focus_log_get(&log,20742).completed==0);
    focus_log_add_completions(&log,20742,2);
    assert(focus_log_valid(&log) && focus_log_get(&log,20742).completed==2);
    focus_log_add_completions(&log,20742,UINT16_MAX);
    assert(focus_log_get(&log,20742).completed==UINT16_MAX);
    focus_log_add_completions(&log,20742+FOCUS_LOG_DAYS,1);
    assert(focus_log_get(&log,20742).completed==0);
    assert(focus_log_get(&log,20742+FOCUS_LOG_DAYS).completed==1);
}

int main(void)
{
    test_dates();
    test_log_ring();
    test_completion_storage_compatibility();
    puts("focus log tests: PASS");
    return 0;
}
