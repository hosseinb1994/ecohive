/******************************************************************************
 * @file           : ADC.h
 * @brief          : Header file for ADC1 driver on STM32F4xx microcontrollers
 * @author         : Hossein Baghaei
 * @date           : 02-Nov-2025
 * @version        : 1.0
 *
 * @details
 * This module provides low-level ADC functionalities for STM32F4xx devices.
 * It includes initialization and functions to read from both external
 * and internal ADC channels such as:
 *  - External analog input (e.g., PA0 - ADC Channel 0)
 *  - Internal temperature sensor (Channel 16)
 *  - Internal reference voltage VREFINT (Channel 17)
 *
 * API Overview:
 *  - ADC_Init()           : Configures GPIOA and ADC1 for analog mode
 *  - ADC_ReadChannel()    : Reads raw ADC value from a selected channel
 *  - ADC_ReadTempSensor() : Reads and converts internal temperature sensor data
 *  - ADC_ReadVref()       : Reads internal reference voltage
 *
 * @note
 *  - The temperature sensor and VREFINT must be enabled through ADC->CCR.TSVREFE.
 *  - The VBAT channel (18) can also be read if battery voltage monitoring is required.
 *  - Default ADC resolution: 12-bit.
 *
 * @reference
 *  - STM32F4xx Reference Manual (RM0090)
 *  - STM32F401 Datasheet, section “ADC Characteristics”
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/
#ifndef INC_ADC_H_
#define INC_ADC_H_

#include "stm32f4xx.h"
#include <stdint.h>

/** @brief Initializes ADC1 and configures PA0 as analog input. */
void ADC_Init(void);

/**
 * @brief Reads the specified ADC channel and returns the raw 12-bit result.
 * @param channel ADC channel number (0–18).
 * @return 12-bit ADC conversion result, or 0xFFFF if the conversion timed out.
 */
uint16_t ADC_ReadChannel(uint8_t channel);

/**
 * @brief Reads the internal temperature sensor and returns temperature in °C.
 * @return Temperature in Celsius.
 */
float ADC_ReadTempSensor(void);

/**
 * @brief Reads internal reference voltage (VREFINT) and returns voltage in volts.
 * @return Reference voltage in Volts.
 */
float ADC_ReadVref(void);

#endif /* INC_ADC_H_ */
