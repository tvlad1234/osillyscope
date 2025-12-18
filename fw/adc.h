#ifndef _ADC_H
#define _ADC_H

#include "stdint.h"

// available ADC clock dividers
extern const uint8_t availableAdcDivs[];

void adc_set_div(uint8_t div);
void init_adc(oscilloscope_t *osc);
void adc_arm_trigger();
void adc_update_trigger();

#endif