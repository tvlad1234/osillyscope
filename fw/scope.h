#ifndef _SCOPE_H
#define _SCOPE_H

#include <stdint.h>

#define DISP_BUF_LEN 64
#define CIRC_BUF_LEN (2 * DISP_BUF_LEN)

#define AN_IN_GPIO GPIOD
#define AN_IN_PIN 5
#define AN_IN_CH 5

#define BTN_R_GPIO GPIOC
#define BTN_R_PIN 6

#define BTN_C_GPIO GPIOC
#define BTN_C_PIN 7

#define BTN_L_GPIO GPIOD
#define BTN_L_PIN 4

#define volts_from_adc(s) ((frontend_offset - (3.3f * s / 1023.0f)) * 2.0f * atten)

enum
{
    TRIG_FALLING,
    TRIG_RISING
};

enum
{
    RUN_AUTO,
    RUN_NORM,
    RUN_END
};

enum
{
    NO_TRIGGER = 0,
    TRIG_FIRST,
    TRIG_SECOND,
    POST_TRIG,
    TRIG_DISABLED
};

extern float atten;
extern float frontend_offset;

struct Oscilloscope
{
    uint8_t runmode;

    uint16_t *wave_buf;
    uint16_t *adc_buf;

    volatile uint16_t trig_level;
    volatile uint8_t trig_slope;
    volatile uint8_t trig_state;
    volatile uint8_t awdg_trigged;

    volatile uint8_t dma_ready;
    volatile uint8_t dma_halves_filled;

    uint8_t tdiv_sel;
    uint8_t vdiv_sel;

    float samp_per;
    float atten;
    float frontend_offset;
};

typedef struct Oscilloscope oscilloscope_t;

void init_oscilloscope(oscilloscope_t *oscope);
void capture_waveform(oscilloscope_t *osc);

#endif