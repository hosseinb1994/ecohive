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
 * through GPIO bit-banging with precise microsecond timing.
 *
 * API Overview:
 *  - AM2302_Init()      : Initializes GPIO and prepares the sensor interface
 *  - AM2302_Start()     : Sends start signal and waits for sensor response
 *  - AM2302_ReadBit()   : Reads a single bit from the sensor
 *  - AM2302_ReadByte()  : Reads a full byte (8 bits)
 *  - AM2302_Read()      : Reads temperature and humidity data, performs checksum validation
 *
 * @note
 * Connect the SDA pin to PB5 with a 4.7kΩ pull-up resistor to 3.3V.
 * Ensure that the SystemCoreClock is configured before calling delay_us().
 *
 * @reference
 * - AM2302/DHT22 Communication Protocol
 * - STM32F4xx Reference Manual
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/

#ifndef INC_AM2302_H_
#define INC_AM2302_H_

#include "stm32f4xx.h"

void delay_us(uint32_t us);
void AM2302_Init(void);
uint8_t AM2302_Start(void);
uint8_t AM2302_ReadBit(void);
uint8_t AM2302_ReadByte(void);
uint8_t AM2302_Read(float *temperature, float *humidity);




#endif /* INC_AM2302_H_ */
