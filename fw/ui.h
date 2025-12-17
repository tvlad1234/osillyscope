#ifndef _UI_H
#define _UI_H

#include "gfx.h"
#include "ssd1306.h"
#include "scope.h"

enum
{
    UI_NONE,
    UI_VDIV,
    UI_ATTEN,
    UI_TDIV,
    UI_TRIGLEV,
    UI_TRIGSLOPE,
    UI_RUNMODE,
    UI_END
};

#define PLOT_WIDTH 64

#define PIXDIV 16
#define YDIV 4
#define XDIV 4

// OLED and GFX instances
extern ssd1306_oled myOled;
extern gfx_inst myGfx;


void init_ui();

// Draws a dotted horizontal line, used for drawing the graticule
void dotted_h_line(gfx_inst *gfx, int x, int y, int l);

// Draws a dotted vertical line, used for drawing the graticule
void dotted_v_line(gfx_inst *gfx, int x, int y, int l);

// Draws graticule, specified number of divisions and pixels/division
void draw_graticule(gfx_inst *gfx, uint16_t divx, uint16_t divy, uint16_t pix);

// Calculates frequency in captured buffer
float measure_frequency(uint16_t *buffer, uint16_t trigger_level, float sample_period_us);

// Oscilloscope UI
void scope_ui(oscilloscope_t *osc);

void boot_splash();

#endif