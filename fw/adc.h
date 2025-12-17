#ifndef _ADC_H
#define _ADC_H

#include "stdint.h"


void adc_set_div(uint8_t div);
void init_adc();
void adc_arm_trigger();

#endif