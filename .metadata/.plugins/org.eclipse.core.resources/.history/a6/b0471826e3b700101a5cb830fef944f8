#include "stm32f4xx.h"  // Ensure to include this header
#include "GPIO.h"


void GPIOA_Init(void)
{
    // 1. Enable GPIOA clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // 2. Set Pin 5 as output mode (MODER register: 01 on bits 10-11)
    GPIOA->MODER &= ~(3U << 10);  // Clear bits 10-11
    GPIOA->MODER |= (1U << 10);   // Set bit 10 to 1 for output mode

    // 3. Set speed for Pin 5 (OSPEEDR register: optional, leave as is if default)
    GPIOA->OSPEEDR &= ~(3U << 10);  // Set to low speed (00)

    // 4. Configure pull-down for Pin 5 (PUPDR register: optional, can be left default)
    GPIOA->PUPDR &= ~(3U << 10);  // Clear bits 10-11
    GPIOA->PUPDR |= (1U << 10);   // Set pull-up (01) on bits 10-11

    // Initial state: Set Pin 5 high (BSRR register)
    GPIOA->BSRR = (1U << 5);  // Set PA5 high
}

void Hearth_beat_ON(void)
{
        // Toggle PA5 using BSRR
        GPIOA->BSRR = (1U << 5);      // Set PA5 high
}

void Hearth_beat_OFF(void)
{
	    GPIOA->BSRR = (1U << (5 + 16));  // Reset PA5 (set bit 5 + 16)
}
