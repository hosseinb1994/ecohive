#include "stm32f4xx.h"
#include "UART.h"

void UART_Init(void)
{
    // Enable GPIOA clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    // Enable USART1 clock (APB2 bus)
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    // Enable DMA2 clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    // Configure PA9 (Tx) as Alternate Function
    GPIOA->MODER &= ~(3U << 18);     // Clear MODER9
    GPIOA->MODER |=  (2U << 18);     // Set PA9 to AF mode

    // Set high speed for PA9
    GPIOA->OSPEEDR |= (2U << 18);

    // Set alternate function AF7 (USART1) for PA9
    GPIOA->AFR[1] &= ~(0xF << 4);    // Clear AF bits for PA9
    GPIOA->AFR[1] |=  (0x7 << 4);    // AF7 = USART1

    // Configure USART1 parameters: 9600 baud, 8 data bits, 1 stop bit
    USART1->BRR = 0x8890;            // 9600 baud @ 16 MHz
    USART1->CR1 |= USART_CR1_TE;     // Enable transmitter
    USART1->CR1 |= USART_CR1_UE;     // Enable USART
}

void UART1_Tx_DMA_Init(void * SrcAddr, uint16_t dataSize)
{
    USART1->CR3 |= USART_CR3_DMAT; // Enable UART DMA

    // Disable DMA stream for configuration
    DMA2_Stream7->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream7->CR & DMA_SxCR_EN);

    // Clear all interrupt flags for Stream 7
    DMA2->HIFCR |= DMA_HIFCR_CDMEIF7 |
                   DMA_HIFCR_CTEIF7  |
                   DMA_HIFCR_CHTIF7  |
                   DMA_HIFCR_CTCIF7;

    // Peripheral address (USART1 data register)
    DMA2_Stream7->PAR = (uint32_t)&USART1->DR;
    // Memory address (your data source)
    DMA2_Stream7->M0AR = (uint32_t)SrcAddr;
    // Number of data to transfer
    DMA2_Stream7->NDTR = dataSize;

    // Configure DMA stream (Stream 7, Channel 4 for USART1 TX)
    DMA2_Stream7->CR = (4U << DMA_SxCR_CHSEL_Pos) |  // Channel 4
                       DMA_SxCR_MINC |               // Memory increment
                       DMA_SxCR_DIR_0 |              // Mem-to-periph
                       DMA_SxCR_TCIE |               // Transfer complete interrupt
                       DMA_SxCR_CIRC;                // Circular mode (optional)

    // Enable DMA stream
    DMA2_Stream7->CR |= DMA_SxCR_EN;
}

void Send_Data(char data)
{
    while (!(USART1->SR & USART_SR_TXE));  // Wait until TXE is set
    USART1->DR = data;                     // Transmit data
    while (!(USART1->SR & USART_SR_TC));   // Wait until TC is set
}

void Print_Message(void * SrcAddr, uint16_t dataSize)
{
    const char *Failure_Msg = "Memory allocation failed\r\n";

    typedef struct
    {
        int Data_Size;
        char Data[100];
    } Data_Transmission;

    Data_Transmission *First_Data = malloc(sizeof(Data_Transmission));
    if (First_Data == NULL)
    {
        Split_Bits(Failure_Msg);
        return;
    }

    strcpy(First_Data->Data, SrcAddr);
    First_Data->Data_Size = dataSize;
    Split_Bits(First_Data->Data);
    free(First_Data);
}

void Split_Bits(const char *ptr)
{
    while (*ptr != '\0')
    {
        Send_Data(*ptr++);
    }
}
