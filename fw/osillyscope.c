/*
 * CH32V003 oscilloscope
 * by tvlad1234
 */

#include "ch32fun.h"

#include "scope.h"
#include "ui.h"

// ADC capture buffers
volatile uint16_t wave_buf[DISP_BUF_LEN] = {0};
volatile uint16_t adc_buf[CIRC_BUF_LEN] = {0}; // circular

oscilloscope_t oscope = {
	.wave_buf = wave_buf,
	.adc_buf = adc_buf,

	.dma_ready = 1,

	.trig_level = 502,
	.trig_state = NO_TRIGGER,

	.tdiv_sel = 1,
	.vdiv_sel = 2,

	.runmode = RUN_AUTO,
};

int main()
{
	SystemInit();
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA;

	// Let power supplies settle before initializing ADC and OLED
	Delay_Ms(250);

	init_oscilloscope(&oscope);

	while (1)
	{
		// Waveform capture
		capture_waveform(&oscope);

		// User interface
		scope_ui(&oscope);
	}
}
