/******************************************************************************
 * @file           : test_Math.c
 * @brief          : Unity unit tests for Core/Src/Math.c (MQ-9 gas sensor math)
 *
 * @details
 * Math.c is the only module in this project with no STM32 register/HAL
 * dependency, so it is the only one that can be compiled and run natively
 * on the host (no hardware, no mocks needed). Expected values are
 * recomputed independently here from the documented formulas rather than
 * copied from the implementation, so the tests actually catch typos such
 * as a flipped sign, swapped operand, or wrong constant.
 ******************************************************************************/
#include "unity.h"
#include "test_Math.h"
#include "Math.h"
#include <math.h>

/* Constants mirrored from Math.c's documented behavior (RL=10k, Vcc=5V,
 * Vref=3.3V, 12-bit ADC) and the CO curve-fit coefficients (m, b). */
#define RL_MQ9   10000.0f
#define VCC_MQ9  5.0f
#define VREF_MCU 3.3f
#define ADC_MAX  4095.0f
#define CURVE_M  (-0.654f)
#define CURVE_B  (1.699f)

#define FLOAT_TOL 0.5f /* absolute tolerance for ohm-range comparisons */

void setUp(void) { }
void tearDown(void) { }

/* --- MQ9_GetRs -------------------------------------------------------- */

void test_MQ9_GetRs_ZeroADC_ReturnsInfinity(void)
{
    /* adc_raw = 0 -> Vout = 0V, guarded by the < 0.001f check */
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, MQ9_GetRs(0));
}

void test_MQ9_GetRs_BelowVoltageThreshold_ReturnsInfinity(void)
{
    /* adc_raw = 1 -> Vout = (1/4095)*3.3 = 0.000806V, still below the
     * 0.001V div-by-zero guard, so this must also short-circuit. */
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, MQ9_GetRs(1));
}

void test_MQ9_GetRs_MidRangeADC_MatchesDividerFormula(void)
{
    uint16_t adc_raw = 2048;
    float vout = (adc_raw / ADC_MAX) * VREF_MCU;
    float expected_rs = RL_MQ9 * (VCC_MQ9 - vout) / vout;

    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, expected_rs, MQ9_GetRs(adc_raw));
}

void test_MQ9_GetRs_MaxADC_MatchesDividerFormula(void)
{
    uint16_t adc_raw = 4095; /* Vout == VREF_MCU exactly */
    float expected_rs = RL_MQ9 * (VCC_MQ9 - VREF_MCU) / VREF_MCU;

    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, expected_rs, MQ9_GetRs(adc_raw));
}

/* --- MQ9_GetPPM --------------------------------------------------------- */

void test_MQ9_GetPPM_ZeroRatio_ReturnsInvalidSentinel(void)
{
    /* Rs_Ro == 0 isn't physically possible for two real resistances, so it
     * must be flagged as invalid rather than silently reported as 0 ppm.
     * Fixed: MQ9_GetPPM() used to return NAN here; it now returns the
     * defined MQ9_PPM_INVALID sentinel (-1.0f) instead, so a fault can
     * never leak a NaN into downstream logging/CSV/ML data (a NaN would
     * corrupt a CSV row and break the ML pipeline's dataset). */
    TEST_ASSERT_TRUE(isfinite(MQ9_GetPPM(0.0f)));
    TEST_ASSERT_EQUAL_FLOAT(MQ9_PPM_INVALID, MQ9_GetPPM(0.0f));
}

void test_MQ9_GetPPM_NegativeRatio_ReturnsInvalidSentinel(void)
{
    TEST_ASSERT_TRUE(isfinite(MQ9_GetPPM(-1.0f)));
    TEST_ASSERT_EQUAL_FLOAT(MQ9_PPM_INVALID, MQ9_GetPPM(-1.0f));
}

void test_MQ9_GetPPM_RatioOfOne_MatchesCurveFormula(void)
{
    float ratio = 1.0f; /* Rs == Ro */
    float expected_ppm = powf(10.0f, (log10f(ratio) - CURVE_B) / CURVE_M);

    TEST_ASSERT_FLOAT_WITHIN(expected_ppm * 0.001f, expected_ppm, MQ9_GetPPM(ratio));
}

void test_MQ9_GetPPM_TypicalRatio_MatchesCurveFormula(void)
{
    float ratio = 0.3f;
    float expected_ppm = powf(10.0f, (log10f(ratio) - CURVE_B) / CURVE_M);

    TEST_ASSERT_FLOAT_WITHIN(expected_ppm * 0.001f, expected_ppm, MQ9_GetPPM(ratio));
}

void test_MQ9_GetPPM_InfiniteRatio_ReturnsInvalidSentinel(void)
{
    /* MQ9_GetRs() can return INFINITY on a near-zero ADC reading
     * (disconnected sensor / fault). MQ9_GetPPM() rejects any non-finite
     * Rs_Ro and returns MQ9_PPM_INVALID instead of silently computing
     * "0 ppm" (which would be indistinguishable from a genuine clean-air
     * reading, since log10f(inf) flips sign after dividing by the
     * negative curve constant m, giving powf(10, -inf) == 0). Returning a
     * finite negative sentinel instead of NAN means this can never
     * corrupt a downstream CSV/ML dataset. */
    TEST_ASSERT_TRUE(isfinite(MQ9_GetPPM(INFINITY)));
    TEST_ASSERT_EQUAL_FLOAT(MQ9_PPM_INVALID, MQ9_GetPPM(INFINITY));
}

/* --- Runner --------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_MQ9_GetRs_ZeroADC_ReturnsInfinity);
    RUN_TEST(test_MQ9_GetRs_BelowVoltageThreshold_ReturnsInfinity);
    RUN_TEST(test_MQ9_GetRs_MidRangeADC_MatchesDividerFormula);
    RUN_TEST(test_MQ9_GetRs_MaxADC_MatchesDividerFormula);

    RUN_TEST(test_MQ9_GetPPM_ZeroRatio_ReturnsInvalidSentinel);
    RUN_TEST(test_MQ9_GetPPM_NegativeRatio_ReturnsInvalidSentinel);
    RUN_TEST(test_MQ9_GetPPM_RatioOfOne_MatchesCurveFormula);
    RUN_TEST(test_MQ9_GetPPM_TypicalRatio_MatchesCurveFormula);
    RUN_TEST(test_MQ9_GetPPM_InfiniteRatio_ReturnsInvalidSentinel);

    return UNITY_END();
}
