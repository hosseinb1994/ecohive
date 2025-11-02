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
static uint32_t SPI_GetPrescaler(uint32_t spi_frequency);
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
    uint32_t prescaler = SPI_GetPrescaler(config->spi_frequency);

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

    // For receive-only, we need to send dummy data
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
static uint32_t SPI_GetPrescaler(uint32_t spi_frequency)
{
    uint32_t apb_frequency;

    // Determine which APB bus the SPI is on and get its frequency
    // For F401RE: SPI1 on APB2 (84 MHz), SPI2/3 on APB1 (42 MHz)
    // You may need to adjust this based on your clock configuration
    if (RCC->APB2ENR & RCC_APB2ENR_SPI1EN) {
        apb_frequency = 84000000; // APB2 frequency
    } else {
        apb_frequency = 42000000; // APB1 frequency
    }

    uint32_t divisor = apb_frequency / spi_frequency;

    if (divisor <= 2) return 0;      // fPCLK/2
    else if (divisor <= 4) return 1; // fPCLK/4
    else if (divisor <= 8) return 2; // fPCLK/8
    else if (divisor <= 16) return 3; // fPCLK/16
    else if (divisor <= 32) return 4; // fPCLK/32
    else if (divisor <= 64) return 5; // fPCLK/64
    else if (divisor <= 128) return 6; // fPCLK/128
    else return 7; // fPCLK/256
}

static void SPI_GPIO_Init(SPI_TypeDef *Instance)
{
    GPIO_TypeDef *gpio;
    uint32_t pin_miso, pin_mosi, pin_sck;
    uint32_t af;

    if (Instance == SPI2) {
        // SPI2 on GPIOC: PC2=MISO, PC3=MOSI, PC10=SCK
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
        gpio = GPIOC;
        pin_miso = 2;
        pin_mosi = 3;
        pin_sck = 10;
        af = 5; // AF5 for SPI2 on these pins
    } else if (Instance == SPI1) {
        // Add other SPI instances as needed
        return;
    } else {
        return;
    }

    // Configure MISO (PC2)
    gpio->MODER &= ~(3U << (pin_miso * 2));
    gpio->MODER |= (2U << (pin_miso * 2)); // Alternate function mode
    gpio->AFR[pin_miso >> 3] &= ~(0xF << ((pin_miso & 7) * 4));
    gpio->AFR[pin_miso >> 3] |= (af << ((pin_miso & 7) * 4));

    // Configure MOSI (PC3)
    gpio->MODER &= ~(3U << (pin_mosi * 2));
    gpio->MODER |= (2U << (pin_mosi * 2)); // Alternate function mode
    gpio->AFR[pin_mosi >> 3] &= ~(0xF << ((pin_mosi & 7) * 4));
    gpio->AFR[pin_mosi >> 3] |= (af << ((pin_mosi & 7) * 4));

    // Configure SCK (PC10)
    gpio->MODER &= ~(3U << (pin_sck * 2));
    gpio->MODER |= (2U << (pin_sck * 2)); // Alternate function mode
    gpio->AFR[pin_sck >> 3] &= ~(0xF << ((pin_sck & 7) * 4));
    gpio->AFR[pin_sck >> 3] |= (af << ((pin_sck & 7) * 4));

    // Configure NSS as GPIO output (you can choose any available pin)
    // Example: Use PC0 as NSS
    gpio->MODER &= ~(3U << (0 * 2));
    gpio->MODER |= (1U << (0 * 2)); // Output mode
    gpio->ODR |= (1U << 0); // Set high initially
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
