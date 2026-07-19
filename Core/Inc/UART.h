/******************************************************************************
 * @file           : UART.h
 * @brief          : UART communication driver header for STM32F4xx series
 * @author         : Hossein Baghaei
 * @date           : 02-Nov-2025
 * @version        : 1.1
 *
 * @details
 * This header provides the public API for UART transmission on STM32F4xx,
 * including support for DMA-based transfers to reduce CPU load.
 *
 * Features:
 *  - UART1 TX communication
 *  - Blocking transmit
 *  - DMA TX option for high-speed continuous transfer
 *  - Helper function for printing strings
 *
 * Recommended Pin Configuration:
 *  - PA9 → USART1_TX (AF7)
 *
 * @note
 * - Ensure UART_Init() is called before any transmission functions.
 * - Baud rate configured for 115200 @ 84 MHz APB2 clock.
 *
 * @reference
 * - STM32F4xx Reference Manual (USART & DMA chapters)
 * - RM0090: DMA2 Stream 7, Channel 4 → USART1_TX mapping
 *
 ******************************************************************************/
#ifndef INC_UART_H_
#define INC_UART_H_

#include <stdlib.h>
#include <string.h>

/**
 * @brief Initializes UART1 peripheral for TX-only communication.
 */
void UART_Init(void);

/**
 * @brief Blocking transmit of a single character.
 */
void Send_Data(char data);

/**
 * @brief Prints a string message via UART (blocking).
 * @param SrcAddr Pointer to a buffer to transmit (need not be null-terminated).
 * @param dataSize Number of characters to send, truncated to the internal
 *                 100-byte buffer if larger.
 */
void Print_Message(void * SrcAddr, uint16_t dataSize);

/**
 * @brief Sends a string buffer character-by-character.
 */
void Split_Bits(const char *ptr);

/**
 * @brief Initializes DMA for UART1 TX transfer.
 * @param SrcAddr Pointer to buffer to transmit.
 * @param dataSize Number of bytes to send.
 */
void UART1_Tx_DMA_Init(void * SrcAddr, uint16_t dataSize);

#endif
