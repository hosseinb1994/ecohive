#include "stm32f4xx.h"
#include "UART.h"

void UART_Init(void)
{
    // Enable GPIOA clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    // Enable USART2 clock
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    //Enable DMA Clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    // Configure PA2 (Tx) as Alternate Function Mode
    GPIOA->MODER &= 0x00000000;  // Clear MODER bits for PA GPIOs
    GPIOA->MODER |= (2U << 4);   // Set PA2 to AF mode

    // Configure PA2 as High Speed
    GPIOA->OSPEEDR |= (2U << 4);

    // Configure AF7 (USART2) for PA2
    GPIOA->AFR[0] &= ~(0xF << 8);  // Clear PA2 AF bits
    GPIOA->AFR[0] |= (0x7 << 8);   // Set PA2 to AF7

    // Configure USART2 parameters: 9600 baud, 8 data bits, 1 stop bit
    USART2->BRR = 0x0683;            // Baud rate 9600 (assuming 16MHz clock)
    USART2->CR1 |= USART_CR1_TE;     // Enable transmitter
    USART2->CR1 |= USART_CR1_UE;     // Enable USART

    USART2->CR3 |= USART_CR3_DMAT; //Enable UART DMA
}

void UART2_Tx_DMA_Init(void * SrcAddr, uint16_t dataSize)
{
	//Disable DMA stream for configuration
	DMA1_Stream6->CR &= ~DMA_SxCR_EN;
	// Wait for stream to be disabled
	while(DMA1_Stream6->CR & DMA_SxCR_EN);

	//Write o in HIFCR
	DMA1->HIFCR &= ~(0xFFFFFFFF);
	//Write 1 in HIFCR related to stream6
	DMA1->HIFCR = DMA_HIFCR_CDMEIF6 |
				  DMA_HIFCR_CTEIF6  |
				  DMA_HIFCR_CHTIF6  |
				  DMA_HIFCR_CTCIF6;
	//Set the peripheral port register address
	DMA1_Stream6->PAR = (uint32_t)&USART2->DR;
	//Set the memory address.
	//Later store ADC values to M0AR
	DMA1_Stream6->M0AR = (uint32_t)SrcAddr;
	//Total number of data items to be transferred
	DMA1_Stream6->NDTR = dataSize;
	// Configure DMA stream for USART2 transmission (DMA1 Stream 6, Channel 4)
	DMA1_Stream6->CR = (4U << DMA_SxCR_CHSEL_Pos)|  // Select Channel 4 for USART2 TX
			DMA_SxCR_MINC | // Enable memory increment mode
			DMA_SxCR_DIR_0 |  // Set direction: memory to peripheral
			DMA_SxCR_TCIE |
			DMA_SxCR_CIRC; // Enable transfer complete interrupt (Circular Mode)
	//Enable DMA stream
	DMA1_Stream6->CR |= DMA_SxCR_EN;

}

void Send_Data(char data)
{
    while (!(USART2->SR & USART_SR_TXE));  // Wait until TXE is set
    USART2->DR = data;                     // Transmit data
    while (!(USART2->SR & USART_SR_TC));   // Wait until TC is set (transmission complete)
}

void Print_Message(void * SrcAddr, uint16_t dataSize)
{
    // Failure message
    const char *Failure_Msg = "Memory allocation failed\r\n";

    // Define structure for data transmission
    typedef struct
    {
        int Data_Size;
        char Data[100];
    } Data_Transmission;

    // Allocate memory for the data structure
    Data_Transmission *First_Data = malloc(sizeof(Data_Transmission));
    if (First_Data == NULL)
    {
        // If memory allocation fails, send failure message
        Split_Bits(Failure_Msg);
        return;
    }

    // Initialize and prepare data for transmission
    strcpy(First_Data->Data, SrcAddr);
    //First_Data->Data_Size = strlen(First_Data->Data);
    First_Data->Data_Size = dataSize;
    // Send data via UART
    Split_Bits(First_Data->Data);

    // Free allocated memory
    free(First_Data);
}

void Split_Bits(const char *ptr)
{
    // Iterate through the string and send each character
    while (*ptr != '\0')
    {
        Send_Data(*ptr++);
    }
}
