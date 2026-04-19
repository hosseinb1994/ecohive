/******************************************************************************
 * @file           : AM2302.h
 * @brief          : Header file for AM2302 (DHT22) temperature and humidity sensor driver
 * @author         : Hossein Baghaei
 * @date           : 29-Oct-2025
 * @version        : 1.0
 *
 * @details
 * This header defines the function prototypes for initializing and reading
 * data from the AM2302 (DHT22) sensor. The communication protocol is handled
 * through GPIO bit-banging with precise microsecond timing using DWT.
 *
 * Hardware Connection:
 * -------------------------------------------------------------
 *  AM2302 Pin   | STM32F401 Pin | Description
 * -------------------------------------------------------------
 *  VDD          | 3.3V          | Power supply
 *  SDA          | PB5           | Data line (with 4.7kΩ pull-up)
 *  NC           | —             | Not connected
 *  GND          | GND           | Ground reference
 * -------------------------------------------------------------
 *
 * @note
 * - Connect the SDA pin to PB5 with a 4.7kΩ pull-up resistor to 3.3V.
 * - Ensure DWT is enabled for accurate timing measurement.
 * - Wait at least 2 seconds after power-up before the first read.
 * - Uses FreeRTOS critical sections for timing precision.
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/

#ifndef INC_AM2302_H_
#define INC_AM2302_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Public Function Prototypes ------------------------------------------------*/

/**
 * @brief Initializes the AM2302 sensor interface
 *
 * Configures GPIO pin PB5 and enables DWT cycle counter for microsecond timing.
 */
void AM2302_Init(void);

/**
 * @brief Reads temperature and humidity from the AM2302 sensor
 *
 * Performs complete communication sequence with the sensor:
 * - Start signal generation
 * - Data reception with precise timing
 * - Checksum validation
 * - Data conversion to physical units
 *
 * @param[out] temperature Pointer to store temperature in °C
 * @param[out] humidity Pointer to store relative humidity in %RH
 * @return uint8_t 1 on success, 0 on failure (communication error or checksum)
 *
 * @note This function enters a FreeRTOS critical section for timing precision.
 *       Minimum interval between reads should be 2 seconds.
 */
uint8_t AM2302_Read(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif /* INC_AM2302_H_ */
