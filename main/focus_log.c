#include "focus_log.h"

#include <string.h>

static unsigned slot_of(int32_t day)
{
    int32_t r = day % FOCUS_LOG_DAYS;
    return (unsigned)(r < 0 ? r + FOCUS_LOG_DAYS : r);
}

void focus_log_init(focus_log_t *log)
{
    memset(log, 0, sizeof(*log));
    log->version = FOCUS_LOG_VERSION;
    for (unsigned i = 0; i < FOCUS_LOG_DAYS; i++) log->slot[i].day = INT32_MIN;  // empty
}

bool focus_log_valid(const focus_log_t *log)
{
    if (log->version != FOCUS_LOG_VERSION) return false;
    for (unsigned i = 0; i < FOCUS_LOG_DAYS; i++) {
        const focus_day_t *s = &log->slot[i];
        if (s->day != INT32_MIN && slot_of(s->day) != i) return false;
        if (s->secs > 86400U) return false;
    }
    return true;
}

void focus_log_add(focus_log_t *log, int32_t day, uint32_t secs, uint16_t sessions)
{
    focus_day_t *s = &log->slot[slot_of(day)];
    if (s->day != day) {  // a new day replaces whatever was there a year ago
        s->day = day;
        s->secs = 0;
        s->sessions = 0;
        s->completed = 0;
    }
    uint32_t total = s->secs + secs;
    s->secs = total > 86400U ? 86400U : total;
    s->sessions = (uint16_t)(s->sessions + sessions < s->sessions ? 0xFFFFU : s->sessions + sessions);
}

void focus_log_add_completions(focus_log_t *log, int32_t day, uint16_t count)
{
    focus_log_add(log, day, 0, 0);
    focus_day_t *s = &log->slot[slot_of(day)];
    uint32_t total = (uint32_t)s->completed + count;
    s->completed = total > UINT16_MAX ? UINT16_MAX : (uint16_t)total;
}

focus_day_t focus_log_get(const focus_log_t *log, int32_t day)
{
    focus_day_t s = log->slot[slot_of(day)];
    if (s.day != day) {
        s.day = day;
        s.secs = 0;
        s.sessions = 0;
        s.completed = 0;
    }
    return s;
}

// Howard Hinnant's days_from_civil / civil_from_days.
int32_t date_to_day(int y, int m, int d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

void date_from_day(int32_t z, int *year, int *month, int *mday)
{
    z += 719468;
    const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int32_t y = (int32_t)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    *year = (int)(y + (m <= 2));
    *month = (int)m;
    *mday = (int)d;
}

int date_weekday_mon0(int32_t day)
{
    int32_t w = (day + 3) % 7;  // 1970-01-01 was a Thursday (Monday-based index 3)
    return (int)(w < 0 ? w + 7 : w);
}

int date_days_in_month(int year, int month)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 29 : 28;
    return days[(month - 1) % 12];
}
