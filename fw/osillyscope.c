/*
 * CH32V003 oscilloscope
 * by tvlad1234
 */

#include "ch32v003fun.h"

#include "scope.h"
#include "ui.h"

int main()
{
	SystemInit();
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA;

	// Let power supplies settle before initializing ADC and OLED
	Delay_Ms(250);

	// Initialize ADC and UI (button GPIO and screen)
	init_adc();
	init_ui();

	// Splash screen
	boot_splash();

	while (1)
	{
		// Waveform capture
		capture_waveform();

		// User interface
		scope_ui();
	}
}
