/******************************************************************************
 * @file           : Math.c
 * @brief          : Implementation of mathematical functions for MQ-9 gas sensor
 * @author         : Hossein Baghaei
 * @date           : 02-Nov-2025
 * @version        : 1.0
 *
 * @details
 * This file implements the conversion formulas to calculate the sensor resistance
 * (Rs) and estimate the CO concentration in parts per million (ppm) based on
 * the MQ-9 gas sensor characteristics.
 *
 * The relationship between Rs/Ro and gas concentration is derived from the
 * datasheet’s log-log characteristic curve:
 *
 *     log(ppm) = (log(Rs/Ro) - b) / m
 *
 * where:
 *   - Rs: Measured sensor resistance
 *   - Ro: Sensor resistance in clean air
 *   - m, b: Empirical coefficients from datasheet curve fitting
 *
 * Hardware Connection:
 * -----------------------------------------------------
 *  MQ9 Pin | STM32 Pin | Description
 *  ---------------------------------------------------
 *  AO      | PA0 (ADC) | Analog output (voltage divider)
 *  DO      | -          | Digital threshold output (optional)
 *  VCC     | 5V         | Power supply
 *  GND     | GND        | Ground
 * -----------------------------------------------------
 *
 * @note
 * - Replace m and b with experimentally derived values for your environment.
 * - Ro_MQ9 should be set after calibration in clean air.
 * - Default RL = 10kΩ (depends on module).
 ******************************************************************************/
#include <math.h>   // for powf()
#include "Math.h"   // include the header

#define RL_MQ9     10000.0f   // RL value on module, usually 10 kΩ (based on datasheet/module)
#define VCC_MQ9    5.0f       // Sensor supply voltage
#define VREF_MCU   3.3f       // STM32 ADC reference voltage
#define ADC_MAX    4095.0f    // 12-bit ADC resolution

// This sensor must be determined experimentally in clean air
float Ro_MQ9 = 10000.0f; // placeholder, to be calibrated

/**
 * @brief Calculates the sensor resistance (Rs) based on ADC reading.
 */
// Convert ADC raw to sensor resistance Rs
float MQ9_GetRs(uint16_t adc_raw)
{
    float Vout = (adc_raw / ADC_MAX) * VREF_MCU;  // AO voltage measured
    if (Vout < 0.001f) return INFINITY;           // avoid div by zero
    return RL_MQ9 * (VCC_MQ9 - Vout) / Vout;      // Rs formula
}

/**
 * @brief Estimates CO gas concentration in ppm using Rs/Ro ratio.
 */
// Estimate ppm (example curve for CO, rough!)
float MQ9_GetPPM(float Rs_Ro)
{
    // These curve values depend on datasheet log-log plots
    // For CO (from MQ9 datasheet curve approximation):
    float m = -0.654;
    float b = 1.699;
    float ratio = Rs_Ro;

    if (ratio <= 0) return 0;

    float ppm_log = (log10f(ratio) - b) / m;
    return powf(10, ppm_log);
}
