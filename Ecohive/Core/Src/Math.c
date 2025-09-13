#include <math.h>   // for powf()
#include "Math.h"   // include the header

#define RL_MQ9     10000.0f   // RL value on your module, usually 10 kΩ (check datasheet/module)
#define VCC_MQ9    5.0f       // Sensor supply voltage
#define VREF_MCU   3.3f       // STM32 ADC reference voltage
#define ADC_MAX    4095.0f

// This must be determined experimentally in clean air
float Ro_MQ9 = 10000.0f; // placeholder, to be calibrated

// Convert ADC raw to sensor resistance Rs
float MQ9_GetRs(uint16_t adc_raw)
{
    float Vout = (adc_raw / ADC_MAX) * VREF_MCU;  // AO voltage measured
    if (Vout < 0.001f) return INFINITY;           // avoid div by zero
    return RL_MQ9 * (VCC_MQ9 - Vout) / Vout;      // Rs formula
}

// Estimate ppm (example curve for CO, rough!)
float MQ9_GetPPM(float Rs_Ro)
{
    // These curve values depend on datasheet log-log plots
    // Example: log(ppm) = (log(Rs/Ro) - b) / m
    // For CO (from MQ9 datasheet curve approximation):
    float m = -0.654;
    float b = 1.699;
    float ratio = Rs_Ro;

    if (ratio <= 0) return 0;

    float ppm_log = (log10f(ratio) - b) / m;
    return powf(10, ppm_log);
}
