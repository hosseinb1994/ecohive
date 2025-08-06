#include "stm32f4xx.h"
#include "ADC.h"

void ADC_Init()
{
	//Enable GPIOA clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

	//Enable ADC1 clock (APB2 bus)
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	//Configure PA0 as Analog Mode
	GPIOA->MODER &= ~(0x3 << 0);  // Clear bits 1:0 for PA0
	GPIOA->MODER |=  (0x3 << 0);  // Set PA0 in Analog mode

	//Disable pull-up/pull-down on PA0
	GPIOA->PUPDR &= ~(0x3 << 0);


}
