#ifndef _UI_H
#define _UI_H

#include "gfx.h"
#include "ssd1306.h"

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

// available volts/division
extern const float availableVoltDiv[];

extern uint8_t ui_selector;
extern uint8_t vdivSel;

void init_ui();

// Draws a dotted horizontal line, used for drawing the graticule
void dotted_h_line(gfx_inst *gfx, int x, int y, int l);

// Draws a dotted vertical line, used for drawing the graticule
void dotted_v_line(gfx_inst *gfx, int x, int y, int l);

// Draws graticule, specified number of divisions and pixels/division
void draw_graticule(gfx_inst *gfx, uint16_t divx, uint16_t divy, uint16_t pix);

// Oscilloscope UI
void scope_ui();

void boot_splash();

#endif