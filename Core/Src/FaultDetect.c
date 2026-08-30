/******************************************************************************
 * @file           : FaultDetect.c
 * @brief          : Implementation of lightweight on-MCU MQ-9 fault detection
 * @author         : Hossein Baghaei
 * @date           : 22-Jul-2026
 * @version        : 1.0
 *
 * @details
 * Pure software module, no HAL/register access and no dynamic memory.
 * A fixed-size ring buffer holds the last FD_WINDOW_SIZE raw ADC samples;
 * running min/max/variance are recomputed over that window each cycle
 * (cheap: window is small and this only runs once per measurement, not in
 * an ISR or tight loop).
 *
 * Tuning: every threshold below is a #define so values can be adjusted for
 * your specific MQ-9 module/circuit without touching the logic.
 ******************************************************************************/
#include "FaultDetect.h"
#include <math.h>

/* ---------------------------------------------------------------------------
 * Tunable thresholds - adjust these to match your hardware and datasheet.
 * ------------------------------------------------------------------------- */

/* STUCK: how many consecutive raw samples ("N") must fall within
 * FD_STUCK_NOISE_ADC counts of each other (peak-to-peak) to call it stuck.
 *
 * IMPORTANT - this window is measured in FaultDetect_Update() calls, which
 * happen once per MQ9_Task loop iteration, i.e. every ~1000ms (MQ9_Task's
 * own vTaskDelay), NOT once per CSV_LOG_INTERVAL_MS (7000ms) row. An earlier
 * tuning pass wrongly assumed the ~7s CSV cadence, so FD_WINDOW_SIZE=30 was
 * actually only covering ~30s of real time (not ~210s) and still produced
 * false STUCK flags on a second clean multi-hour log - the MQ-9's analog
 * output is slow/stable enough that ~30s of quiet is well within normal
 * behavior. FD_WINDOW_SIZE is now sized against the correct ~1s cadence,
 * with a large safety margin (the CSV log - sampled only every ~7th update -
 * still showed runs of up to 13 identical consecutive rows, i.e. ~91s with
 * no visible change at the 7s-spaced granularity we can actually observe;
 * the true 1s-spaced runs underneath are plausibly much longer). At
 * window=300 and ~1s/sample, worst-case detection latency for a genuinely
 * frozen/disconnected sensor is ~300s (~5 min), which is an acceptable
 * trade for zero false positives per your priority. */
#define FD_WINDOW_SIZE          300
#define FD_STUCK_NOISE_ADC      1      /* ADC counts of allowed wiggle       */

/* SATURATED: raw ADC at/near the 12-bit rails (0..4095). Widened from an
 * earlier 5/4090 cutoff after hardware testing showed a dead/grounded
 * sensor sitting at e.g. raw=6 was mathematically "valid-looking" once run
 * through the Rs/ppm formula (small but finite ppm) and slipped past the
 * RANGE check - catching it here, directly on the raw ADC, is the correct
 * layer to defend against that. */
#define FD_SAT_LOW_ADC          20
#define FD_SAT_HIGH_ADC         4075

/* OUT-OF-RANGE: MQ-9 datasheet CO detection range is ~10-1000 ppm.
 * FD_PPM_MIN is set to 0 (not 10) so low-but-plausible readings near the
 * bottom of the sensing range are not falsely flagged. */
#define FD_PPM_MIN              0.0f
#define FD_PPM_MAX              1000.0f

/* RATE-SPIKE: max plausible ppm change between two consecutive measurement
 * cycles. MQ-9 gas response is a slow chemical process (seconds), so a huge
 * jump between reads points at a glitch, not real gas dynamics. */
#define FD_MAX_PPM_RATE         500.0f

/* ---------------------------------------------------------------------------
 * State (module-static, no dynamic memory)
 * ------------------------------------------------------------------------- */
static uint16_t s_ring[FD_WINDOW_SIZE];
static uint8_t  s_ring_index = 0;
static uint8_t  s_ring_count = 0;

static uint16_t s_last_min = 0;
static uint16_t s_last_max = 0;
static float    s_last_variance = 0.0f;

static float    s_prev_ppm = 0.0f;
static uint8_t  s_have_prev_ppm = 0;

void FaultDetect_Init(void)
{
    s_ring_index = 0;
    s_ring_count = 0;
    s_have_prev_ppm = 0;
    s_last_min = 0;
    s_last_max = 0;
    s_last_variance = 0.0f;

    for (uint8_t i = 0; i < FD_WINDOW_SIZE; i++)
    {
        s_ring[i] = 0;
    }
}

uint8_t FaultDetect_Update(uint16_t raw_adc, float ppm)
{
    uint8_t flags = 0;

    /* --- push the new sample into the ring buffer --- */
    s_ring[s_ring_index] = raw_adc;
    s_ring_index = (uint8_t)((s_ring_index + 1) % FD_WINDOW_SIZE);
    if (s_ring_count < FD_WINDOW_SIZE)
    {
        s_ring_count++;
    }

    /* --- running min/max/variance over the samples currently in the window --- */
    uint16_t min_v = s_ring[0];
    uint16_t max_v = s_ring[0];
    uint32_t sum   = 0;
    uint32_t sumsq = 0;

    for (uint8_t i = 0; i < s_ring_count; i++)
    {
        uint16_t v = s_ring[i];
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
        sum   += v;
        sumsq += (uint32_t)v * (uint32_t)v;
    }

    float mean = (float)sum / (float)s_ring_count;
    float variance = ((float)sumsq / (float)s_ring_count) - (mean * mean);
    if (variance < 0.0f) variance = 0.0f; /* guard against float rounding */

    s_last_min = min_v;
    s_last_max = max_v;
    s_last_variance = variance;

    /* --- STUCK: window full and peak-to-peak spread within noise budget --- */
    if (s_ring_count == FD_WINDOW_SIZE && (uint16_t)(max_v - min_v) <= FD_STUCK_NOISE_ADC)
    {
        flags |= FAULT_STUCK_BIT;
    }

    /* --- SATURATED: raw ADC parked at/near a rail --- */
    if (raw_adc < FD_SAT_LOW_ADC || raw_adc > FD_SAT_HIGH_ADC)
    {
        flags |= FAULT_SATURATED_BIT;
    }

    /* --- OUT-OF-RANGE: non-finite (NAN/INF) or outside datasheet bounds ---
     * !isfinite() is checked and OR'd in FIRST and short-circuits the rest:
     * comparisons against NaN (e.g. "NaN < 0") always evaluate to false in
     * C, so a naive "ppm < FD_PPM_MIN || ppm > FD_PPM_MAX" test on its own
     * would silently let a NaN through. isfinite() is what actually catches
     * NaN and +/-Inf here. In practice MQ9_GetPPM() is now written to never
     * return NaN/Inf in the first place (see Math.c), so this is a
     * defense-in-depth backstop rather than the primary catch. */
    if (!isfinite(ppm) || ppm < FD_PPM_MIN || ppm > FD_PPM_MAX)
    {
        flags |= FAULT_RANGE_BIT;
    }

    /* --- RATE-SPIKE: implausible jump since the previous cycle --- */
    if (s_have_prev_ppm && isfinite(ppm) && isfinite(s_prev_ppm))
    {
        float delta = ppm - s_prev_ppm;
        if (delta < 0.0f) delta = -delta;
        if (delta > FD_MAX_PPM_RATE)
        {
            flags |= FAULT_RATE_BIT;
        }
    }

    if (isfinite(ppm))
    {
        s_prev_ppm = ppm;
        s_have_prev_ppm = 1;
    }

    return flags;
}

void FaultDetect_GetStats(uint16_t *min_out, uint16_t *max_out, float *variance_out)
{
    if (min_out)      *min_out = s_last_min;
    if (max_out)      *max_out = s_last_max;
    if (variance_out) *variance_out = s_last_variance;
}
