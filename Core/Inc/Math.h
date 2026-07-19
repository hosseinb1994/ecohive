/******************************************************************************
 * @file           : Math.h
 * @brief          : Mathematical functions for MQ-9 gas sensor data processing
 * @author         : Hossein Baghaei
 * @date           : 02-Nov-2025
 * @version        : 1.0
 *
 * @details
 * This module provides utility functions to calculate sensor resistance (Rs)
 * and estimate gas concentration (PPM) for the MQ-9 gas sensor.
 *
 * The conversion is based on the sensor's voltage divider model and empirical
 * data derived from the MQ-9 datasheet log-log sensitivity curves.
 *
 * API Overview:
 *  - MQ9_GetRs(adc_raw)  : Computes Rs from ADC raw value.
 *  - MQ9_GetPPM(Rs_Ro)   : Estimates gas concentration (CO in ppm).
 *
 * @note
 * - `Ro_MQ9` should be calibrated in clean air before using the PPM calculation.
 * - The formula and coefficients (m, b) are approximations for CO detection.
 *
 * @reference
 * - MQ-9 Gas Sensor Datasheet (Hanwei Electronics)
 * - STM32F4xx Reference Manual (ADC conversion principles)
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/
#ifndef INC_MATH_H_
#define INC_MATH_H_

#include <stdint.h>

/**
 * @brief Global calibration resistance value (Ro) for MQ9 in clean air.
 * @note  Should be experimentally determined and stored for accuracy.
 */
extern float Ro_MQ9;  // global calibration variable

/**
 * @brief Computes the sensor resistance (Rs) from raw ADC measurement.
 * @param adc_raw Raw ADC value from the analog output (0–4095 for 12-bit ADC).
 * @return Calculated sensor resistance in ohms.
 */
float MQ9_GetRs(uint16_t adc_raw);

/**
 * @brief Estimates CO gas concentration in ppm from Rs/Ro ratio.
 * @param Rs_Ro Ratio of sensor resistance (Rs) to calibration resistance (Ro).
 * @return Estimated gas concentration in ppm.
 */
float MQ9_GetPPM(float Rs_Ro);


#endif /* INC_MATH_H_ */
