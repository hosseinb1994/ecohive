/******************************************************************************
 * @file           : GPIO.c
 * @brief          : GPIOA driver implementation for LED heartbeat control
 * @author         : Hossein Baghaei
 * @date           : 02-Nov-2025
 * @version        : 1.0
 *
 * @details
 * This file provides low-level functions for configuring and controlling
 * GPIOA pin 5 on STM32F4xx microcontrollers. It enables the GPIOA clock,
 * configures PA5 as output, and provides simple ON/OFF control using the
 * Bit Set/Reset Register (BSRR).
 *
 * Hardware Connection:
 * ---------------------------------------------------
 *  STM32 Pin | Signal | Description
 *  --------------------------------------------------
 *  PA5       | LED     | Heartbeat indicator
 * ---------------------------------------------------
 *
 * @note
 * - The LED is active high (logic ‘1’ = ON).
 * - The default configuration sets PA5 to low-speed output mode.
 *
 * @todo
 * - Add support for GPIO pin configuration via parameters.
 * - Add GPIO toggle functionality (e.g., Heartbeat_Toggle()).
 ******************************************************************************/
#include "stm32f4xx.h"  // Ensure to include this header
#include "GPIO.h"

/**
 * @brief Initializes GPIOA pin 5 as a push-pull output for LED control.
 */
void GPIOA_Init(void)
{
    // 1. Enable GPIOA clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // 2. Set Pin 5 as output mode (MODER register: 01 on bits 10-11)
    GPIOA->MODER &= ~(3U << 10);  // Clear bits 10-11
    GPIOA->MODER |= (1U << 10);   // Set bit 10 to 1 for output mode

    // 3. Set speed for Pin 5 (OSPEEDR register: optional, leave as is if default)
    GPIOA->OSPEEDR &= ~(3U << 10);  // Set to low speed (00)

    // 4. Configure pull-up for Pin 5 (PUPDR register: optional, can be left default)
    GPIOA->PUPDR &= ~(3U << 10);  // Clear bits 10-11
    GPIOA->PUPDR |= (1U << 10);   // Set pull-up (01) on bits 10-11

    // Initial state: Set Pin 5 high (BSRR register)
    GPIOA->BSRR = (1U << 5);  // Set PA5 high
}

/**
 * @brief Turns ON the heartbeat LED (PA5 high).
 */
void Hearth_beat_ON(void)
{
        // Toggle PA5 using BSRR
        GPIOA->BSRR = (1U << 5);      // Set PA5 high
}

/**
 * @brief Turns OFF the heartbeat LED (PA5 low).
 */
void Hearth_beat_OFF(void)
{
	    GPIOA->BSRR = (1U << (5 + 16));  // Reset PA5 (set bit 5 + 16)
}
