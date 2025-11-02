/******************************************************************************
 * @file           : SPI.h
 * @brief          : SPI peripheral driver header file for STM32F4xx series
 * @author         : Hossein Baghaei
 * @date           : 29-Oct-2025
 * @version        : 1.0
 *
 * @details
 * This header file defines the data structures, enumerations, and function
 * prototypes used by the SPI driver. It provides configuration options for
 * SPI parameters such as frequency, data size, bit order, polarity, and phase.
 *
 * The driver offers a simple and efficient API for SPI communication without
 * the overhead of HAL or LL libraries, making it suitable for embedded
 * applications requiring precise control over SPI timing and registers.
 *
 * @note
 * Ensure that SPI_Init() is called before using any communication functions.
 * Clock and GPIO configuration is handled internally.
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/


#ifndef INC_SPI_H_
#define INC_SPI_H_

#include <stdint.h>
#include <stdbool.h>

// SPI Configuration Structure
typedef struct {
    uint32_t spi_frequency;    // SPI clock frequency in Hz
    uint8_t data_size;         // 8 or 16 bits
    bool lsb_first;           // LSB first if true, MSB first if false
    bool cpol;                // Clock polarity: 0=low idle, 1=high idle
    bool cpha;                // Clock phase: 0=first edge, 1=second edge
    bool crc_enable;          // Enable CRC calculation
    uint16_t crc_polynomial;  // CRC polynomial value
} SPI_Config;

// SPI Handle Structure
typedef struct {
    SPI_TypeDef *Instance;    // SPI peripheral instance
    SPI_Config config;        // SPI configuration
    bool initialized;         // Initialization status
} SPI_Handle;

// Function prototypes
void SPI_Init(SPI_Handle *hspi, SPI_TypeDef *Instance, SPI_Config *config);
void SPI_DeInit(SPI_Handle *hspi);
bool SPI_Transmit(SPI_Handle *hspi, uint8_t *pData, uint16_t Size);
bool SPI_Receive(SPI_Handle *hspi, uint8_t *pData, uint16_t Size);
bool SPI_TransmitReceive(SPI_Handle *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size);
bool SPI_CheckCRCError(SPI_Handle *hspi);
void SPI_ClearCRC(SPI_Handle *hspi);

// Helper functions
void SPI_Enable(SPI_Handle *hspi);
void SPI_Disable(SPI_Handle *hspi);
void SPI_SetNSS(SPI_Handle *hspi, bool state);


#endif /* INC_SPI_H_ */
