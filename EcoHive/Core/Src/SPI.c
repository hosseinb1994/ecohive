#include "stm32f4xx.h"
#include "SPI.h"
#include "UART.h"

void SPI_Init(void)
{
    // Clock GPIOA was enabled in UART driver
	// Enable GPIOB clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    // Enable SPI1 clock
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    //Enable DMA Clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    // Configure GPIOA5,6,7 and GPIOB6 as Alternate Function Mode
    GPIOB->MODER &= 0x00000000;
    GPIOA->MODER |= (2U << 10) | (2U << 12) | (2U << 14);   // Set PA5,6,7 to AF mode
    GPIOB->MODER |= (1U << 12);   // Set PB6 to general output mode as Chip Select
    GPIOB->BSRR = (1U << 6);                             // Set PB6 high (CS idle)
    // Configure PA5,6,7 as very High Speed and PB6 as High Speed
    GPIOA->OSPEEDR |= (3U << 10) | (3U << 12) | (3U << 14);
    GPIOB->OSPEEDR |= (2U << 12);
    //Set PB6 as Pull-Down
    GPIOB->PUPDR |= (2U << 12);
    // Configure AF5 (SPI1) for PA5,6,7
    GPIOA->AFR[0] &= ~((0xF << (5 * 4)) | (0xF << (6 * 4)) | (0xF << (7 * 4)));  // Clear PA5,6,7 AF bits
    GPIOA->AFR[0] |= ((0x5 << (5 * 4)) | (0x5 << (6 * 4)) | (0x5 << (7 * 4)));   // Set PA5,6,7 to AF5

    // Configure SPI1
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_DFF | SPI_CR1_BR_0; // Master mode, software NSS, 16-bit, baud rate
    SPI1->CR2 = SPI_CR2_TXDMAEN;   // Enable TX DMA
    SPI1->CR1 |= SPI_CR1_SPE;      // Enable SPI1
}

void SPI1_Tx_DMA_Init(void * SrcAddr, uint16_t dataSize)
{
	Print_Message((void *)"Start SPI DMA config\r\n", 30);
	//Disable DMA stream for configuration
	DMA2_Stream3->CR &= ~DMA_SxCR_EN;
	// Wait for stream to be disabled
	while (DMA2_Stream3->CR & DMA_SxCR_EN);

	//Write o in LIFCR
	DMA2->LIFCR &= ~(0xFFFFFFFF);
	//Write 1 and clear in LIFCR related to stream3
	DMA2->LIFCR = DMA_LIFCR_CDMEIF3 |
				  DMA_LIFCR_CTEIF3  |
				  DMA_LIFCR_CHTIF3  |
				  DMA_LIFCR_CTCIF3;
	//Set the peripheral port register address
	DMA2_Stream3->PAR = (uint32_t)&SPI1->DR;
	// Set DMA stream priority
	//DMA2_Stream3->CR |= DMA_SxCR_PL_1;
	//Set the memory address.
	//Later store ADC values to M0AR
	DMA2_Stream3->M0AR = (uint32_t)SrcAddr;
	//Total number of data items to be transferred
	DMA2_Stream3->NDTR = dataSize;
	// Configure DMA stream for SPI1 transmission (DMA2 Stream 3, Channel 3)
	DMA2_Stream3->CR = (3U << DMA_SxCR_CHSEL_Pos)|  // Select Channel 3 for USART2 TX
			DMA_SxCR_MINC | // Enable memory increment mode
			DMA_SxCR_DIR_0 |  // Set direction: memory to peripheral
			DMA_SxCR_TCIE |
			DMA_SxCR_CIRC; // Enable transfer complete interrupt (Circular Mode)
	//Enable DMA stream
	DMA2_Stream3->CR |= DMA_SxCR_EN;
	Print_Message((void *)"SPI DMA Enabled...\r\n", 30);
}
