#include "charge_detect.h"

#include <string.h>

void charge_detect_init(charge_detect_t *c)
{
    memset(c, 0, sizeof(*c));
}

static int median3(int a, int b, int x)
{
    if (a > b) { int t = a; a = b; b = t; }
    if (b > x) b = x;
    return a > b ? a : b;
}

// i = 0 is the newest filtered sample, i = count - 1 the oldest.
static int at(const charge_detect_t *c, int i)
{
    return c->mv[(c->head + CHARGE_WINDOW - 1 - i) % CHARGE_WINDOW];
}

static int mean(const charge_detect_t *c, int from, int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++) sum += at(c, from + i);
    return sum / n;
}

bool charge_detect_sample(charge_detect_t *c, int mv, bool usb_host)
{
    if (mv < 0) return usb_host || c->charging;

    c->raw[c->raw_next] = (int16_t)mv;
    c->raw_next = (uint8_t)((c->raw_next + 1) % 3);
    if (c->raw_count < 3) c->raw_count++;
    if (c->raw_count < 3) return usb_host || c->charging;
    int filtered = median3(c->raw[0], c->raw[1], c->raw[2]);

    c->mv[c->head] = (int16_t)filtered;
    c->head = (uint8_t)((c->head + 1) % CHARGE_WINDOW);
    if (c->count < CHARGE_WINDOW) c->count++;

    // Step: compare with the extremes of the previous ~30 s.
    int span = c->count - 1 < CHARGE_STEP_SPAN ? c->count - 1 : CHARGE_STEP_SPAN;
    if (span > 0) {
        int lo = at(c, 1), hi = at(c, 1);
        for (int i = 2; i <= span; i++) {
            int v = at(c, i);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        int step = filtered - lo >= CHARGE_STEP_MV ? 1 : hi - filtered >= CHARGE_STEP_MV ? -1 : 0;
        if (step) {
            c->charging = step > 0;
            // Restart history at the new level so the slow trend below does not
            // keep reacting to the step it has just explained.
            c->count = 1;
            return usb_host || c->charging;
        }
    }

    // Trend: average of the newest 5 against the oldest 5 over two minutes.
    if (c->count == CHARGE_WINDOW) {
        int delta = mean(c, 0, 5) - mean(c, CHARGE_WINDOW - 5, 5);
        if (delta >= CHARGE_TREND_MV) c->charging = true;
        else if (delta <= -CHARGE_TREND_MV) c->charging = false;
    }
    return usb_host || c->charging;
}
