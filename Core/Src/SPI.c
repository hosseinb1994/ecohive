/******************************************************************************
 * @file           : SPI.c
 * @brief          : SPI peripheral driver implementation for STM32F4xx series
 * @author         : Hossein Baghaei
 * @date           : 29-Oct-2025
 * @version        : 1.0
 *
 * @details
 * This file contains the low-level implementation of the SPI (Serial Peripheral
 * Interface) driver for STM32F4xx microcontrollers. It supports initialization,
 * data transmission, reception, full-duplex communication, and CRC verification.
 *
 * The driver provides a lightweight alternative to HAL/LL libraries and allows
 * fine-grained control over the SPI peripheral for performance-critical or
 * bare-metal applications.
 *
 * Features:
 *  - SPI Master mode operation
 *  - Configurable data size (8/16-bit)
 *  - Adjustable clock polarity (CPOL) and phase (CPHA)
 *  - LSB/MSB first data order
 *  - CRC computation and error checking
 *  - Software-controlled NSS (Chip Select)
 *
 * @note
 * - Tested on STM32F401RE (SPI1, SPI2)
 * - Compatible with STM32F4xx standard peripheral registers
 * - Designed for use with custom board-level GPIO configuration
 *
 * @todo
 * - Add support for DMA-based SPI transfers
 * - Extend GPIO configuration for SPI1 and SPI3
 *
 * @copyright
 * (c) 2025 Hossein Baghaei. All rights reserved.
 ******************************************************************************/


#include "stm32f4xx.h"
#include "SPI.h"

// Private function prototypes
static uint32_t SPI_GetPrescaler(SPI_TypeDef *Instance, uint32_t spi_frequency);
static void SPI_GPIO_Init(SPI_TypeDef *Instance);
static void SPI_Clock_Enable(SPI_TypeDef *Instance);

void SPI_Init(SPI_Handle *hspi, SPI_TypeDef *Instance, SPI_Config *config)
{
    // Validate parameters
    if (hspi == NULL || Instance == NULL || config == NULL) {
        return;
    }

    // Initialize handle
    hspi->Instance = Instance;
    hspi->config = *config;

    // Enable clocks
    SPI_Clock_Enable(Instance);

    // Initialize GPIO
    SPI_GPIO_Init(Instance);

    // Disable SPI before configuration
    hspi->Instance->CR1 &= ~SPI_CR1_SPE;

    // Calculate baud rate prescaler
    uint32_t prescaler = SPI_GetPrescaler(Instance, config->spi_frequency);

    // Configure SPI CR1 register
    uint32_t cr1 = 0;
    cr1 |= SPI_CR1_MSTR;                    // Master mode
    cr1 |= (prescaler << SPI_CR1_BR_Pos);   // Baud rate
    cr1 |= (config->cpol ? SPI_CR1_CPOL : 0); // Clock polarity
    cr1 |= (config->cpha ? SPI_CR1_CPHA : 0); // Clock phase
    cr1 |= (config->lsb_first ? SPI_CR1_LSBFIRST : 0); // Frame format

    if (config->data_size == 16) {
        cr1 |= SPI_CR1_DFF;                 // 16-bit data frame
    }

    // Software NSS management
    cr1 |= SPI_CR1_SSM;                     // Software slave management
    cr1 |= SPI_CR1_SSI;                     // Internal slave select

    hspi->Instance->CR1 = cr1;

    // Configure CRC if enabled
    if (config->crc_enable) {
        hspi->Instance->CRCPR = config->crc_polynomial;
        hspi->Instance->CR1 |= SPI_CR1_CRCEN;
    }

    // Enable SPI
    hspi->Instance->CR1 |= SPI_CR1_SPE;

    hspi->initialized = true;
}

void SPI_DeInit(SPI_Handle *hspi)
{
    if (hspi == NULL || hspi->Instance == NULL) {
        return;
    }

    // Disable SPI
    hspi->Instance->CR1 &= ~SPI_CR1_SPE;

    // Reset configuration
    hspi->Instance->CR1 = 0;
    hspi->Instance->CR2 = 0;

    hspi->initialized = false;
}

bool SPI_Transmit(SPI_Handle *hspi, uint8_t *pData, uint16_t Size)
{
    if (hspi == NULL || pData == NULL || Size == 0 || !hspi->initialized) {
        return false;
    }

    for (uint16_t i = 0; i < Size; i++) {
        // Wait until TX buffer is empty
        while (!(hspi->Instance->SR & SPI_SR_TXE));

        // Send data
        if (hspi->config.data_size == 16) {
            uint16_t data = *((uint16_t*)pData);
            hspi->Instance->DR = data;
            pData += 2;
            i++; // Skip extra byte for 16-bit data
        } else {
            hspi->Instance->DR = *pData++;
        }

        // Wait until transmission complete
        while (!(hspi->Instance->SR & SPI_SR_TXE));
        while (hspi->Instance->SR & SPI_SR_BSY);
    }

    return true;
}

bool SPI_Receive(SPI_Handle *hspi, uint8_t *pData, uint16_t Size)
{
    if (hspi == NULL || pData == NULL || Size == 0 || !hspi->initialized) {
        return false;
    }

    // For receive-only, dummy data need to be sent
    for (uint16_t i = 0; i < Size; i++) {
        // Wait until TX buffer is empty
        while (!(hspi->Instance->SR & SPI_SR_TXE));

        // Send dummy data to generate clock
        if (hspi->config.data_size == 16) {
            hspi->Instance->DR = 0xFFFF;
        } else {
            hspi->Instance->DR = 0xFF;
        }

        // Wait until RX buffer is not empty
        while (!(hspi->Instance->SR & SPI_SR_RXNE));

        // Read received data
        if (hspi->config.data_size == 16) {
            *((uint16_t*)pData) = hspi->Instance->DR;
            pData += 2;
            i++; // Skip extra byte for 16-bit data
        } else {
            *pData++ = hspi->Instance->DR;
        }
    }

    return true;
}

bool SPI_TransmitReceive(SPI_Handle *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
    if (hspi == NULL || pTxData == NULL || pRxData == NULL || Size == 0 || !hspi->initialized) {
        return false;
    }

    for (uint16_t i = 0; i < Size; i++) {
        // Wait until TX buffer is empty
        while (!(hspi->Instance->SR & SPI_SR_TXE));

        // Send data
        if (hspi->config.data_size == 16) {
            uint16_t tx_data = *((uint16_t*)pTxData);
            hspi->Instance->DR = tx_data;
            pTxData += 2;
            i++; // Skip extra byte for 16-bit data
        } else {
            hspi->Instance->DR = *pTxData++;
        }

        // Wait until RX buffer is not empty
        while (!(hspi->Instance->SR & SPI_SR_RXNE));

        // Read received data
        if (hspi->config.data_size == 16) {
            *((uint16_t*)pRxData) = hspi->Instance->DR;
            pRxData += 2;
        } else {
            *pRxData++ = hspi->Instance->DR;
        }
    }

    // Wait for all transmissions to complete
    while (hspi->Instance->SR & SPI_SR_BSY);

    return true;
}

bool SPI_CheckCRCError(SPI_Handle *hspi)
{
    if (hspi == NULL || !hspi->initialized) {
        return false;
    }

    return (hspi->Instance->SR & SPI_SR_CRCERR) != 0;
}

void SPI_ClearCRC(SPI_Handle *hspi)
{
    if (hspi == NULL || !hspi->initialized) {
        return;
    }

    // Clear CRC error flag by reading SR and then DR
    volatile uint32_t temp = hspi->Instance->SR;
    temp = hspi->Instance->DR;
    (void)temp; // Prevent unused variable warning
}

void SPI_Enable(SPI_Handle *hspi)
{
    if (hspi != NULL && hspi->initialized) {
        hspi->Instance->CR1 |= SPI_CR1_SPE;
    }
}

void SPI_Disable(SPI_Handle *hspi)
{
    if (hspi != NULL && hspi->initialized) {
        hspi->Instance->CR1 &= ~SPI_CR1_SPE;
    }
}

// Private functions
static uint32_t SPI_GetPrescaler(SPI_TypeDef *Instance, uint32_t spi_frequency)
{
    if (spi_frequency == 0) {
        return 7; // invalid request: fall back to the slowest possible clock
    }

    uint32_t apb_frequency = (Instance == SPI1) ? 84000000UL : 42000000UL;

    uint32_t divisor = apb_frequency / spi_frequency;
    if      (divisor <= 2)   return 0;
    else if (divisor <= 4)   return 1;
    else if (divisor <= 8)   return 2;
    else if (divisor <= 16)  return 3;
    else if (divisor <= 32)  return 4;
    else if (divisor <= 64)  return 5;
    else if (divisor <= 128) return 6;
    else                     return 7;
}

static void SPI_GPIO_Init(SPI_TypeDef *Instance)
{
    if (Instance == SPI2) {
        // SPI2 pins on STM32F401:
        //   PB10 = SCK  (AF5)
        //   PB14 = MISO (AF5)
        //   PB15 = MOSI (AF5)
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
        GPIO_TypeDef *gpio = GPIOB;
        uint32_t af = 5;

        // SCK = PB10 (AFR[1], index 10-8=2)
        gpio->MODER  &= ~(3U << (10 * 2));
        gpio->MODER  |=  (2U << (10 * 2));   // Alternate function
        gpio->OTYPER &= ~(1U << 10);          // Push-pull
        gpio->OSPEEDR|=  (3U << (10 * 2));    // High speed
        gpio->PUPDR  &= ~(3U << (10 * 2));    // No pull
        gpio->AFR[1] &= ~(0xFU << ((10 - 8) * 4));
        gpio->AFR[1] |=  (af   << ((10 - 8) * 4));  // AF5

        // MISO = PB14 (AFR[1], index 14-8=6)
        gpio->MODER  &= ~(3U << (14 * 2));
        gpio->MODER  |=  (2U << (14 * 2));
        gpio->OTYPER &= ~(1U << 14);
        gpio->OSPEEDR|=  (3U << (14 * 2));
        gpio->PUPDR  &= ~(3U << (14 * 2));
        gpio->PUPDR  |=  (1U << (14 * 2));    // Pull-up on MISO
        gpio->AFR[1] &= ~(0xFU << ((14 - 8) * 4));
        gpio->AFR[1] |=  (af   << ((14 - 8) * 4));

        // MOSI = PB15 (AFR[1], index 15-8=7)
        gpio->MODER  &= ~(3U << (15 * 2));
        gpio->MODER  |=  (2U << (15 * 2));
        gpio->OTYPER &= ~(1U << 15);
        gpio->OSPEEDR|=  (3U << (15 * 2));
        gpio->PUPDR  &= ~(3U << (15 * 2));
        gpio->AFR[1] &= ~(0xFU << ((15 - 8) * 4));
        gpio->AFR[1] |=  (af   << ((15 - 8) * 4));

        // CS = PC0 is configured separately in SPI2_Init()
    }
    else if (Instance == SPI1) {
        // SPI1 pins on STM32F401:
        //   PA5 = SCK  (AF5)
        //   PA6 = MISO (AF5)
        //   PA7 = MOSI (AF5)
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
        GPIO_TypeDef *gpio = GPIOA;
        uint32_t af = 5;

        // SCK = PA5
        gpio->MODER  &= ~(3U << (5 * 2));
        gpio->MODER  |=  (2U << (5 * 2));   // Alternate function
        gpio->OTYPER &= ~(1U << 5);          // Push-pull
        gpio->OSPEEDR|=  (3U << (5 * 2));    // High speed
        gpio->PUPDR  &= ~(3U << (5 * 2));    // No pull
        gpio->AFR[0] &= ~(0xFU << (5 * 4));
        gpio->AFR[0] |=  (af   << (5 * 4));  // AF5

        // MISO = PA6
        gpio->MODER  &= ~(3U << (6 * 2));
        gpio->MODER  |=  (2U << (6 * 2));
        gpio->OTYPER &= ~(1U << 6);
        gpio->OSPEEDR|=  (3U << (6 * 2));
        gpio->PUPDR  &= ~(3U << (6 * 2));
        gpio->PUPDR  |=  (1U << (6 * 2));    // Pull-up on MISO
        gpio->AFR[0] &= ~(0xFU << (6 * 4));
        gpio->AFR[0] |=  (af   << (6 * 4));

        // MOSI = PA7
        gpio->MODER  &= ~(3U << (7 * 2));
        gpio->MODER  |=  (2U << (7 * 2));
        gpio->OTYPER &= ~(1U << 7);
        gpio->OSPEEDR|=  (3U << (7 * 2));
        gpio->PUPDR  &= ~(3U << (7 * 2));
        gpio->AFR[0] &= ~(0xFU << (7 * 4));
        gpio->AFR[0] |=  (af   << (7 * 4));

        // CS is application-managed via a plain GPIO pin (see main.c), same
        // as SPI2 above.
    }
}

static void SPI_Clock_Enable(SPI_TypeDef *Instance)
{
    if (Instance == SPI1) {
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    } else if (Instance == SPI2) {
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    } else if (Instance == SPI3) {
        RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
    }
}
