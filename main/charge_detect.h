// main/charge_detect.h - infer "external power / charging" (pure C, host-testable).
//
// The Passport has no charger status line to the MCU (bsp_pins.h; every free
// GPIO is used) and the CW2017 gauge reports only voltage and SOC, no current.
// Charging is therefore inferred from two signals:
//   1. a USB host is enumerating the device (USB-Serial-JTAG start-of-frame),
//      which means 5 V is present - immediate and reliable, computers only;
//   2. the cell-voltage trend: plugging a charger in lifts the voltage by tens
//      of mV within seconds, unplugging drops it the same way, and a slow
//      steady rise/fall over two minutes settles the boot-time state.
// Samples are median-of-3 filtered so a short load spike (the chime) cannot
// fake a plug-in step.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CHARGE_SAMPLE_MS 2000U   // feed one voltage sample this often
#define CHARGE_WINDOW 60         // samples kept: two minutes
#define CHARGE_STEP_MV 25        // plug-in / unplug step within ~30 s
#define CHARGE_STEP_SPAN 15      // samples (30 s) checked for a step
#define CHARGE_TREND_MV 12       // slow rise / fall over the full window

typedef struct {
    int16_t raw[3];              // last raw samples, for the median filter
    uint8_t raw_next, raw_count;
    int16_t mv[CHARGE_WINDOW];   // filtered history, ring buffer
    uint8_t head, count;
    bool charging;               // trend-based state (USB host is ORed on top)
} charge_detect_t;

void charge_detect_init(charge_detect_t *c);
// Feed one sample every CHARGE_SAMPLE_MS. mv < 0 (read failed) keeps the state.
// Returns true when the battery icon should show the charging bolt.
bool charge_detect_sample(charge_detect_t *c, int mv, bool usb_host);
