/******************************************************************************
 * @file           : AM2302.c
 * @brief          : AM2302 (DHT22) Temperature and Humidity Sensor Driver
 * @author         : Hossein Baghaei
 * @date           : 29-Oct-2025
 * @version        : 1.0
 *
 * @details
 * This file contains the low-level implementation of the AM2302 (DHT22) driver
 * for STM32F4xx microcontrollers. It communicates with the sensor using a
 * single-wire protocol to retrieve temperature and humidity data with
 * precise timing control.
 *
 * The driver includes:
 *  - Start signal generation
 *  - Bit-level data reading with microsecond accuracy
 *  - Checksum validation
 *  - Conversion of raw data into temperature (°C) and humidity (%RH)
 *  - Debug logging via UART interface
 *
 * Hardware Connection:
 * -------------------------------------------------------------
 *  AM2302 Pin   | STM32F401 Pin | Description
 * -------------------------------------------------------------
 *  VDD          | 5V            | Power supply
 *  SDA          | PB5           | Data line (with 4.7kΩ pull-up)
 *  NC           | —             | Not connected
 *  GND          | GND           | Ground reference
 * -------------------------------------------------------------
 *
 * @note
 * - This driver uses HAL GPIO APIs and DWT cycle counter for microsecond delay.
 * - Ensure DWT is enabled for accurate timing measurement.
 * - Wait at least 2 seconds after power-up before the first read.
 * - For debugging, UART is used to print messages via Print_Message().
 *
 * @todo
 * - Add support for multiple sensor instances
 * - Integrate non-blocking read or timer-based polling
 *
 * @reference
 * - AM2302/DHT22 Datasheet, Aosong Electronics Co., Ltd.
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/

#include "AM2302.h"
#include "UART.h"
#include "FreeRTOS.h"
#include "task.h"

// Using PB5 for AM2302 SDA
#define AM2302_SDA_PIN GPIO_PIN_5
#define AM2302_SDA_PORT GPIOB

// Timing definitions
#define AM2302_START_LOW_TIME_US 1200  // At least 1ms, using 1.2ms to be safe
#define AM2302_RESPONSE_TIMEOUT_US 100 // Should be ~80us

// Helper macro for DWT-based delay
#define DWT_DELAY_US(us) do { \
    uint32_t startTick = DWT->CYCCNT; \
    uint32_t delayTicks = (us) * (SystemCoreClock / 1000000); \
    while ((DWT->CYCCNT - startTick) < delayTicks); \
} while(0)

// Internal helper to read the pin state
static inline uint8_t AM2302_ReadPin(void) {
    return (uint8_t)HAL_GPIO_ReadPin(AM2302_SDA_PORT, AM2302_SDA_PIN);
}

// Internal helper to set pin as output
static void AM2302_SetOutput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = AM2302_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(AM2302_SDA_PORT, &GPIO_InitStruct);
}

// Internal helper to set pin as input with pull-up
static void AM2302_SetInput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = AM2302_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(AM2302_SDA_PORT, &GPIO_InitStruct);
}

void AM2302_Init(void) {
	// 1. Enable GPIOB clock safely (doesn't hurt if already on)
	__HAL_RCC_GPIOB_CLK_ENABLE();

	// 2. Enable DWT only if not already enabled
	  if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
		  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	      DWT->CYCCNT = 0;
	      DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	   }

	 // 3. Set PB5 as Input with PULLUP (Important for AM2302)
	 AM2302_SetInput();
    // NOTE: The initial 2s delay should be handled in the task, not here.
}

static uint8_t AM2302_Start(void) {
	// Ensure line is high before we start
	    AM2302_SetInput();
	    DWT_DELAY_US(100);

	    // Pull low for 1.2ms
	    AM2302_SetOutput();
	    HAL_GPIO_WritePin(AM2302_SDA_PORT, AM2302_SDA_PIN, GPIO_PIN_RESET);
	    DWT_DELAY_US(1200);

	    // Release and wait for sensor
	    HAL_GPIO_WritePin(AM2302_SDA_PORT, AM2302_SDA_PIN, GPIO_PIN_SET);
	    AM2302_SetInput();

	    // The sensor should pull low within 20-40us
	    uint32_t timeout = 200;
	    while (AM2302_ReadPin() == GPIO_PIN_SET) {
	        if (--timeout == 0) return 0;
	        DWT_DELAY_US(1);
	    }

	    // Now wait for it to go back HIGH (the 80us response pulse)
	    timeout = 200;
	    while (AM2302_ReadPin() == GPIO_PIN_RESET) {
	        if (--timeout == 0) return 0;
	        DWT_DELAY_US(1);
	    }

	    // Wait for it to go back LOW (start of data)
	    timeout = 200;
	    while (AM2302_ReadPin() == GPIO_PIN_SET) {
	        if (--timeout == 0) return 0;
	        DWT_DELAY_US(1);
	    }

	    return 1;
}

static uint8_t AM2302_ReadByte(void) {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        // Wait for the start of the bit (line goes HIGH after the 50us LOW start)
        uint32_t timeout = 100; // > 50us
        while (AM2302_ReadPin() == GPIO_PIN_RESET) {
            if (--timeout == 0) return 0xFF; // Timeout waiting for bit start
            DWT_DELAY_US(1);
        }

        // Measure the length of the HIGH pulse to determine bit value
        // '0' is ~26-28us HIGH, '1' is ~70us HIGH
        DWT_DELAY_US(40); // Wait past the '0' threshold

        if (AM2302_ReadPin() == GPIO_PIN_SET) {
            // It's a '1'
            byte |= (1 << (7 - i));
            // Wait for line to go LOW for the next bit
            timeout = 100;
            while (AM2302_ReadPin() == GPIO_PIN_SET) {
                 if (--timeout == 0) return 0xFF; // Timeout
                 DWT_DELAY_US(1);
            }
        }
        // Else it's a '0', the loop will naturally wait for the next LOW-to-HIGH transition
    }
    return byte;
}

uint8_t AM2302_Read(float *temperature, float *humidity) {
    uint8_t data[5] = {0};

    // --- ENTER CRITICAL SECTION ---
    // Disable interrupts to ensure precise timing
    taskENTER_CRITICAL();

    if (!AM2302_Start()) {
        taskEXIT_CRITICAL();
        return 0; // Start failed
    }

    // Read 5 bytes (40 bits)
    for (int i = 0; i < 5; i++) {
        data[i] = AM2302_ReadByte();
        if (data[i] == 0xFF) {
            taskEXIT_CRITICAL();
            return 0; // Read timeout
        }
    }

    // --- EXIT CRITICAL SECTION ---
    // Re-enable interrupts
    taskEXIT_CRITICAL();

    // Verify checksum
    uint8_t calc_checksum = data[0] + data[1] + data[2] + data[3];
    if (calc_checksum != data[4]) {
        return 0; // Checksum error
    }

    // Convert humidity (16-bit, MSB first)
    uint16_t hum_raw = (data[0] << 8) | data[1];
    *humidity = hum_raw / 10.0f;

    // Convert temperature (16-bit, MSB first)
    uint16_t temp_raw = (data[2] << 8) | data[3];
    if (temp_raw & 0x8000) {
        *temperature = -((temp_raw & 0x7FFF) / 10.0f);
    } else {
        *temperature = temp_raw / 10.0f;
    }

    return 1; // Success
}
