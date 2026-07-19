/******************************************************************************
 * @file           : test_Math.h
 * @brief          : Unity test declarations for Core/Src/Math.c (MQ-9 sensor math)
 ******************************************************************************/
#ifndef TEST_TEST_MATH_H_
#define TEST_TEST_MATH_H_

/* Unity fixtures, called by the framework before/after every test case */
void setUp(void);
void tearDown(void);

/* MQ9_GetRs() cases */
void test_MQ9_GetRs_ZeroADC_ReturnsInfinity(void);
void test_MQ9_GetRs_BelowVoltageThreshold_ReturnsInfinity(void);
void test_MQ9_GetRs_MidRangeADC_MatchesDividerFormula(void);
void test_MQ9_GetRs_MaxADC_MatchesDividerFormula(void);

/* MQ9_GetPPM() cases */
void test_MQ9_GetPPM_ZeroRatio_ReturnsNaN(void);
void test_MQ9_GetPPM_NegativeRatio_ReturnsNaN(void);
void test_MQ9_GetPPM_RatioOfOne_MatchesCurveFormula(void);
void test_MQ9_GetPPM_TypicalRatio_MatchesCurveFormula(void);
void test_MQ9_GetPPM_InfiniteRatio_ReturnsNaN(void);

#endif /* TEST_TEST_MATH_H_ */
