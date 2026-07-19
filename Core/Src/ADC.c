/******************************************************************************
 * @file           : ADC.c
 * @brief          : ADC1 driver implementation for STM32F4xx microcontrollers
 * @author         : Hossein Baghaei
 * @date           : 02-Nov-2025
 * @version        : 1.0
 *
 * @details
 * This file provides implementation for ADC1 initialization and reading from
 * external and internal channels on STM32F4xx series.
 *
 * The driver configures PA0 (ADC Channel 0) as analog input and enables
 * internal temperature sensor and VREFINT for monitoring device parameters.
 *
 * Supported Channels:
 *  ---------------------------------------------------
 *  Channel | Source
 *  ---------------------------------------------------
 *  0       | External analog input (PA0)
 *  16      | Internal temperature sensor
 *  17      | Internal reference voltage (VREFINT)
 *  18      | Battery voltage (VBAT, optional)
 *  ---------------------------------------------------
 *
 * @note
 *  - The temperature calculation uses factory calibration constants:
 *      * V25 = 0.76V (at 25°C)
 *      * Avg_Slope = 2.5 mV/°C
 *  - ADC1 clock must be enabled on APB2 bus before conversion.
 *
 * @todo
 *  - Add multi-channel scanning mode
 *  - Implement DMA-based data acquisition
 ******************************************************************************/
#include "ADC.h"
#include <math.h>   // for NAN

/**
 * @brief Initializes GPIOA and ADC1 for single-channel analog conversion.
 */
void ADC_Init()
{
	//Enable GPIOA clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

	//Enable ADC1 clock (APB2 bus)
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	//Configure PA0 as Analog Mode
	GPIOA->MODER &= ~(0x3 << 0);  // Clear bits 1:0 for PA0
	GPIOA->MODER |=  (0x3 << 0);  // Set PA0 in Analog mode

	//Disable pull-up/pull-down on PA0
	GPIOA->PUPDR &= ~(0x3 << 0);

	// ADC configuration
	ADC1->CR1 &= ~(3 << 24); // 12-bit resolution
	ADC1->CR2 = 0;           // Right alignment, single conversion
	ADC1->SQR3 = 0;          // First conversion in regular sequence = channel 0

    // Enable temperature sensor and VREFINT
    ADC->CCR |= ADC_CCR_TSVREFE;

    // Set long sample times for internal channels
    // Temp sensor (channel 16), VREFINT (channel 17), VBAT (channel 18)
    ADC1->SMPR1 |= (7u << 18); // channel 16 max sample time
    ADC1->SMPR1 |= (7u << 21); // channel 17 max sample time
    ADC1->SMPR1 |= (7u << 24); // channel 18 max sample time

    // External channel 0 also long sample (safe)
    ADC1->SMPR2 |= (7u << 0);

    // Enable ADC1
    ADC1->CR2 |= ADC_CR2_ADON;

}

/**
 * @brief Reads a single ADC channel.
 * @param channel ADC channel number (0–18)
 * @return 12-bit ADC conversion result
 */
/*
ADC_ReadChannel(0) → measure IN0 (PA0)

ADC_ReadChannel(16) → measure temperature sensor

ADC_ReadChannel(17) → measure VREFINT

ADC_ReadChannel(18) → measure VBAT
*/
uint16_t ADC_ReadChannel(uint8_t channel)
{
    // Select channel in regular sequence register
    ADC1->SQR3 = channel & 0x1F;

    // Start conversion
    ADC1->CR2 |= ADC_CR2_SWSTART;

    // Wait until conversion complete, bounded so a stalled ADC can't hang forever
    uint32_t timeout = 1000000U;
    while (!(ADC1->SR & ADC_SR_EOC))
    {
        if (--timeout == 0)
        {
            return 0xFFFFU; // out of the valid 12-bit range: signals a conversion timeout
        }
    }

    return (uint16_t)ADC1->DR;
}

/**
 * @brief Reads internal temperature sensor and returns temperature in Celsius.
 * @return Temperature in °C
 */
// === Internal channels ===

// Temperature sensor (channel 16)
float ADC_ReadTempSensor(void)
{
    uint16_t raw = ADC_ReadChannel(16);

    // Estimate Vdda using VREFINT
    uint16_t vref_raw = ADC_ReadChannel(17);
    if (vref_raw == 0) return NAN; // avoid div by zero (e.g. ADC read timeout)
    float Vdda = 1.21f * 4095.0f / (float)vref_raw;

    // Voltage on temp sensor
    float Vsense = (raw * Vdda) / 4095.0f;

    float V25 = 0.76f;         // voltage at 25°C
    float Avg_Slope = 0.0025f; // 2.5 mV/°C

    return ((Vsense - V25) / Avg_Slope) + 25.0f;
}

/**
 * @brief Reads internal reference voltage (VREFINT).
 * @return Reference voltage in Volts
 */
// VREFINT (channel 17)
float ADC_ReadVref(void)
{
    uint16_t raw = ADC_ReadChannel(17);
    return (3.3f * raw) / 4095.0f; // Vdda = 3.3 V
}
