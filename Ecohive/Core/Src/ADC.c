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

	// ADC configuration
	ADC1->CR1 &= ~(3 << 24); // 12-bit resolution
	ADC1->CR2 = 0;           // Right alignment, single conversion
	ADC1->SQR3 = 0;          // First conversion in regular sequence = channel 0

	// Enable ADC1
	ADC1->CR2 |= ADC_CR2_ADON;

	// Enable internal channels (temperature sensor + VREFINT + VBAT)
	ADC->CCR |= ADC_CCR_TSVREFE | ADC_CCR_VBATE;

}
/*
ADC_ReadChannel(0) → measure IN0 (PA0)

ADC_ReadChannel(16) → measure temperature sensor

ADC_ReadChannel(17) → measure VREFINT

ADC_ReadChannel(18) → measure VBAT
*/
uint16_t ADC_ReadChannel(uint8_t channel)
{
    // Select channel in regular sequence register
    ADC1->SQR3 = channel & 0x1F;

    // Start conversion
    ADC1->CR2 |= ADC_CR2_SWSTART;

    // Wait until conversion complete
    while (!(ADC1->SR & ADC_SR_EOC));

    return (uint16_t)ADC1->DR;
}

// === Internal channels ===

// Temperature sensor (IN18)
float ADC_ReadTempSensor(void)
{
    uint16_t raw = ADC_ReadChannel(18);
    float V_sense = (3.3f * raw) / 4095.0f; // convert ADC to volts

    // Datasheet values
    float V25 = 0.76f;        // V at 25 °C
    float Avg_Slope = 0.0025f; // 2.5 mV/°C

    return ((V_sense - V25) / Avg_Slope) + 25.0f;
}

// VREFINT (IN17)
float ADC_ReadVref(void)
{
    uint16_t raw = ADC_ReadChannel(17);
    return (3.3f * raw) / 4095.0f;
}
