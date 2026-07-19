/******************************************************************************
 * @file           : GPIO.h
 * @brief          : Header file for GPIOA configuration and LED control functions
 * @author         : Hossein Baghaei
 * @date           : 02-Nov-2025
 * @version        : 1.0
 *
 * @details
 * This module provides basic GPIO functionality for STM32F4xx devices.
 * It includes initialization for GPIOA pin 5 (commonly connected to the on-board
 * user LED on Nucleo and Discovery boards) and provides simple control functions
 * to turn the heartbeat LED ON or OFF.
 *
 * API Overview:
 *  - GPIOA_Init()       : Initializes GPIOA pin 5 as push-pull output.
 *  - Hearth_beat_ON()   : Turns ON the heartbeat LED (PA5 HIGH).
 *  - Hearth_beat_OFF()  : Turns OFF the heartbeat LED (PA5 LOW).
 *
 * @note
 * - GPIOA pin 5 is connected to LD2 on Nucleo-F401RE and similar STM32 boards.
 * - This module can be reused for other GPIO pins with minimal modification.
 *
 * @reference
 * - STM32F4xx Reference Manual (RM0090)
 * - STM32F401RE Datasheet, section "I/O Ports"
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/
#ifndef INC_GPIO_H_
#define INC_GPIO_H_


/**
 * @brief Initializes GPIOA Pin 5 as output for LED heartbeat.
 */
void GPIOA_Init(void);

/**
 * @brief Turns ON the heartbeat LED (sets PA5 high).
 */
void Hearth_beat_ON(void);

/**
 * @brief Turns OFF the heartbeat LED (resets PA5 low).
 */
void Hearth_beat_OFF(void);


#endif
