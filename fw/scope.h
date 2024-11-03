#ifndef _SCOPE_H
#define _SCOPE_H

#include <stdint.h>

// ADC capture buffers
#define BUFFER_LENGTH 64
extern volatile uint16_t buffer1[BUFFER_LENGTH];
extern volatile uint16_t buffer2[BUFFER_LENGTH];

// DMA ready (conversion done) flag
extern volatile uint8_t dma_ready;

#define AN_IN_GPIO GPIOD
#define AN_IN_PIN 5
#define AN_IN_CH 5

#define BTN_R_GPIO GPIOC
#define BTN_R_PIN 6

#define BTN_C_GPIO GPIOC
#define BTN_C_PIN 7

#define BTN_L_GPIO GPIOD
#define BTN_L_PIN 4

#define RISING 1
#define FALLING 0

#define volts_from_adc(s) ((frontend_offset - (3.3f * s / 1023.0f)) * 2.0f * atten)

enum
{
    RUN_AUTO,
    RUN_NORM,
    RUN_END
};

// available ADC clock dividers
extern const uint8_t availableAdcDivs[];

// pointers to the two buffers
extern volatile uint16_t *writeBuffer;
extern volatile uint16_t *readBuffer;

// trigger settings and flags
extern volatile uint16_t trigLevel;
extern volatile uint8_t trig;
extern volatile uint8_t awdg_trigged;
extern uint8_t scope_trigged;
extern volatile uint8_t trig_sm;

extern volatile int wf_cnt;
extern float sampPer;
extern float atten;
extern float frontend_offset;

extern uint8_t tdivSel;

// Sampling mode
extern uint8_t runmode;

void capture_waveform();
void adc_set_div(uint8_t div);

// Calculates frequency in captured buffer
float measure_frequency(uint16_t *buffer, uint16_t trigger_level, float sample_period_us);

#endif