#include "stm32f411xe.h"


#define GPIOAEN			(1U<<0)
#define LED_PIN			(1U<<5)


void led_init(void)
{
	/*Enable clock access to led port (Port A)*/
	RCC->AHB1ENR  |= GPIOAEN;

	/*Set led pin as output pin*/
	GPIOA->MODER |=(1U<<10);
	GPIOA->MODER &= ~(1U<<11);
}

void led_on(void)
{
	/*Set led pin HIGH (PA5)*/
	GPIOA->ODR |= LED_PIN;
}

void led_off(void)
{
	/*Set led pin LOW (PA5)*/
	GPIOA->ODR &= ~LED_PIN;
}

void led_toggle(void)
{
	GPIOA->ODR ^= LED_PIN;
}

