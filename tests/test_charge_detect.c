#include "charge_detect.h"

#include <assert.h>
#include <stdio.h>

static charge_detect_t c;
static unsigned seed = 1;

// Deterministic +-3 mV noise, about what the CW2017 VCELL reading shows.
static int noise(void)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % 7) - 3;
}

static bool feed(int mv, int samples, bool usb)
{
    bool on = false;
    for (int i = 0; i < samples; i++) on = charge_detect_sample(&c, mv + noise(), usb);
    return on;
}

static void test_steady_discharge_is_not_charging(void)
{
    charge_detect_init(&c);
    bool on = false;
    for (int i = 0; i < 300; i++) on = charge_detect_sample(&c, 3900 - i / 20 + noise(), false);  // slow sag
    assert(!on);
}

static void test_plug_in_and_unplug_steps(void)
{
    charge_detect_init(&c);
    assert(!feed(3820, 20, false));
    bool on = feed(3900, 3, false);  // charger connected: voltage steps up
    assert(on);
    assert(feed(3905, 40, false));   // stays on while charging
    on = feed(3830, 3, false);       // unplugged: voltage steps back down
    assert(!on);
}

static void test_short_load_spike_is_filtered(void)
{
    charge_detect_init(&c);
    feed(3850, 20, false);
    assert(!charge_detect_sample(&c, 3760, false));  // chime dips the cell once
    assert(!charge_detect_sample(&c, 3850, false));  // recovery is not a plug-in
    assert(!feed(3850, 10, false));
}

static void test_slow_rise_detected_after_boot(void)
{
    // Booted while already charging: no step, but a steady rise.
    charge_detect_init(&c);
    bool on = false;
    for (int i = 0; i < 70; i++) on = charge_detect_sample(&c, 3950 + i / 3, false);
    assert(on);
}

static void test_usb_host_and_failed_reads(void)
{
    charge_detect_init(&c);
    assert(charge_detect_sample(&c, -1, true));
    assert(!charge_detect_sample(&c, -1, false));
    assert(feed(3900, 10, true));
    assert(!feed(3900, 10, false));
}

int main(void)
{
    test_steady_discharge_is_not_charging();
    test_plug_in_and_unplug_steps();
    test_short_load_spike_is_filtered();
    test_slow_rise_detected_after_boot();
    test_usb_host_and_failed_reads();
    puts("charge detect tests: PASS");
    return 0;
}
