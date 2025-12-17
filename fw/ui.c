#include "ch32fun.h"

#include "gfx.h"
#include "ssd1306.h"

#include "ui.h"
#include "scope.h"
#include "adc.h"

// OLED and GFX instances
ssd1306_oled myOled;
gfx_inst myGfx;

// available volts/division
const float availableVoltDiv[] = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 0.0f};
uint8_t ui_selector = UI_NONE;

// link the oled to the gfx
void oled_link_gfx(ssd1306_oled *oled, gfx_inst *gfx)
{
    gfx->disp_ptr = oled;
    gfx->height = oled->height;
    gfx->width = oled->width;
    gfx->pixel_draw_fun = oled_draw_pixel;
    gfx->flush_fun = oled_flush;

    gfx_set_text_color(gfx, WHITE, BLACK);
    gfx_set_text_size(gfx, 1);
    gfx_set_cursor(gfx, 0, 0);
}

// Initialize UI
void init_ui()
{
    BTN_C_GPIO->CFGLR &= ~(0xf << (4 * BTN_C_PIN));
    BTN_C_GPIO->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * BTN_C_PIN);
    BTN_C_GPIO->BSHR = 1 << (BTN_C_PIN);

    BTN_L_GPIO->CFGLR &= ~(0xf << (4 * BTN_L_PIN));
    BTN_L_GPIO->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * BTN_L_PIN);
    BTN_L_GPIO->BSHR = 1 << (BTN_L_PIN);

    BTN_R_GPIO->CFGLR &= ~(0xf << (4 * BTN_R_PIN));
    BTN_R_GPIO->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * BTN_R_PIN);
    BTN_R_GPIO->BSHR = 1 << (BTN_R_PIN);

    oled_init(&myOled, 0x3C, SSD1306_SWITCHCAPVCC, 128, 64);
    oled_link_gfx(&myOled, &myGfx);
}

// Draws a dotted horizontal line, used for drawing the graticule
void dotted_h_line(gfx_inst *gfx, int x, int y, int l)
{
    for (int i = 0; i <= l; i++)
    {
        if (i % 2)
            gfx_draw_pixel(gfx, x + i, y, WHITE);
        else
            gfx_draw_pixel(gfx, x + i, y, BLACK);
    }
}

// Draws a dotted vertical line, used for drawing the graticule
void dotted_v_line(gfx_inst *gfx, int x, int y, int l)
{
    for (int i = 0; i <= l; i++)
    {
        if (i % 2)
            gfx_draw_pixel(gfx, x, y + i, WHITE);
        else
            gfx_draw_pixel(gfx, x, y + i, BLACK);
    }
}

// Draws graticule, specified number of divisions and pixels/division
void draw_graticule(gfx_inst *gfx, uint16_t divx, uint16_t divy, uint16_t pix)
{
    uint16_t wit = divx * pix;
    uint16_t hei = divy * pix;
    for (int i = 0; i <= wit; i += pix)
        dotted_v_line(gfx, i, 0, hei);
    for (int i = 0; i <= hei; i += pix)
        dotted_h_line(gfx, 0, i, wit);
}

// Oscilloscope UI
void scope_ui(oscilloscope_t *osc)
{
    // Handle buttons presses every 100 ms
    uint16_t currentBtnMs = SysTick->CNT / DELAY_MS_TIME;
    static uint16_t prevBtnMs;
    if (currentBtnMs < prevBtnMs)
        prevBtnMs = currentBtnMs;

    if (currentBtnMs - prevBtnMs > 100)
    {

        if (!(BTN_C_GPIO->INDR & (1 << BTN_C_PIN)))
        {
            ui_selector++;
            if (ui_selector == UI_END)
                ui_selector = UI_NONE;
        }

        if (!(BTN_L_GPIO->INDR & (1 << BTN_L_PIN)))
        {
            /*
            if (!(BTN_R_GPIO->INDR & (1 << BTN_R_PIN))) // do DC offset calibration when L+R are pressed
            {
                // capture a whole waveform buffer
                if (dma_ready)
                    //run_dma();
                    ;
                while (!dma_ready)
                    ;

                // then average out the values
                float avgVal = 0;
                for (int i = 0; i < ((DISP_BUF_LEN)-1); i++)
                {
                    avgVal += writeBuffer[i];
                }
                avgVal /= (float)((DISP_BUF_LEN)-1);
                frontend_offset = 3.3f * avgVal / 1023.0f;

                gfx_clear(&myGfx);
                gfx_set_text_color(&myGfx, BLACK, WHITE);
                gfx_print_string(&myGfx, "DC offset calibration\n\n");
                gfx_set_text_color(&myGfx, WHITE, BLACK);
                gfx_print_string(&myGfx, "Offset: ");
                gfx_print_float(&myGfx, frontend_offset);
                gfx_print_string(&myGfx, "V\nMin input: ");
                gfx_print_float(&myGfx, volts_from_adc(0));
                gfx_print_string(&myGfx, "V\nMax input: ");
                gfx_print_float(&myGfx, volts_from_adc(1023));
                gfx_print_string(&myGfx, "V");
                gfx_flush(&myGfx);
                while (BTN_C_GPIO->INDR & (1 << BTN_C_PIN))
                    ;
                ;
            }
            */

            switch (ui_selector)
            {

            case UI_VDIV:
                if (osc->vdiv_sel > 0)
                    osc->vdiv_sel--;
                break;

            case UI_ATTEN:
                atten = 1.0f;
                break;

            case UI_TDIV:
                if (osc->tdiv_sel > 0)
                {
                    osc->tdiv_sel--;
                    adc_set_div(availableAdcDivs[osc->tdiv_sel]);
                }
                break;

            case UI_TRIGLEV:
                if (osc->trig_level > 10)
                    osc->trig_level -= 10;
                break;

            case UI_TRIGSLOPE:
                osc->trig_slope = TRIG_FALLING;
                break;

            case UI_RUNMODE:
                if (osc->runmode > 0)
                    osc->runmode--;
                break;

            default:
                break;
            }
        }

        if (!(BTN_R_GPIO->INDR & (1 << BTN_R_PIN)))
        {
            switch (ui_selector)
            {

            case UI_VDIV:
                osc->vdiv_sel++;
                if (availableVoltDiv[osc->vdiv_sel] == 0.0f)
                    osc->vdiv_sel--;
                break;

            case UI_ATTEN:
                atten = 10.0f;
                break;

            case UI_TDIV:
                osc->tdiv_sel++;
                if (!availableAdcDivs[osc->tdiv_sel])
                    osc->tdiv_sel--;
                adc_set_div(availableAdcDivs[osc->tdiv_sel]);
                break;

            case UI_TRIGLEV:
                if (osc->trig_level < 1013)
                    osc->trig_level += 10;
                break;

            case UI_TRIGSLOPE:
                osc->trig_slope = TRIG_RISING;
                break;

            case UI_RUNMODE:
                osc->runmode++;
                if (osc->runmode == RUN_END)
                    osc->runmode--;
                break;

            default:
                break;
            }
        }
        prevBtnMs = currentBtnMs;
    }

    // Display results
    draw_graticule(&myGfx, XDIV, YDIV, PIXDIV);
    dotted_h_line(&myGfx, 0, (YDIV * PIXDIV) - 1, XDIV * PIXDIV);

    // hack to zoom in onto the waveform at the fastest sample rate (extend time base down to 5us/d)
    if (osc->tdiv_sel == 0)
    {
        int srcIndex = (DISP_BUF_LEN / 2) - 1;
        int dstIndex = DISP_BUF_LEN - 1;

        for (int i = 0; i < DISP_BUF_LEN / 2; i++)
        {
            osc->wave_buf[dstIndex--] = osc->wave_buf[srcIndex];
            osc->wave_buf[dstIndex--] = osc->wave_buf[srcIndex--];
        }
    }

    // Plot the waveform and retrieve voltage min, max and avg
    float vmin = 0;
    float vmax = 0;
    float vAvg = 0;
    for (int i = 0; i < ((PLOT_WIDTH)-1); i++)
    {

        float v1 = volts_from_adc(osc->wave_buf[i]);
        float v2 = volts_from_adc(osc->wave_buf[i + 1]);

        uint16_t h1 = (PIXDIV * YDIV / 2 - 1) - (v1 * PIXDIV / availableVoltDiv[osc->vdiv_sel]);
        uint16_t h2 = (PIXDIV * YDIV / 2 - 1) - (v2 * PIXDIV / availableVoltDiv[osc->vdiv_sel]);

        gfx_draw_line(&myGfx, i, h1, i + 1, h2, WHITE);

        if (v1 < vmin || vmin == 0)
            vmin = v1;

        if (v1 > vmax)
            vmax = v1;

        vAvg += v1;
    }
    vAvg /= (float)((PLOT_WIDTH)-1);

    float measuredFreq = measure_frequency(osc->wave_buf, osc->trig_level, osc->samp_per);

    gfx_set_cursor(&myGfx, 66, 0);
    gfx_print_float(&myGfx, vmin);
    gfx_print_string(&myGfx, "V min");

    gfx_set_cursor(&myGfx, 66, 8);
    gfx_print_float(&myGfx, vmax);
    gfx_print_string(&myGfx, "V max");

    gfx_set_cursor(&myGfx, 66, 16);
    // gfx_printf(&myGfx, "%d kHz ", (int)measuredFreq / 1000);
    gfx_print_float(&myGfx, measuredFreq / 1000.0f);
    gfx_print_string(&myGfx, " kHz");

    gfx_draw_fast_h_line(&myGfx, 4 * PIXDIV + 2, 24, 127 - (4 * PIXDIV + 2), WHITE);

    if (ui_selector == UI_VDIV)
        gfx_set_text_color(&myGfx, BLACK, WHITE);
    else
        gfx_set_text_color(&myGfx, WHITE, BLACK);

    gfx_set_cursor(&myGfx, 66, 27);
    gfx_print_float(&myGfx, availableVoltDiv[osc->vdiv_sel]);
    gfx_print_string(&myGfx, "V/d");

    if (ui_selector == UI_ATTEN)
        gfx_set_text_color(&myGfx, BLACK, WHITE);
    else
        gfx_set_text_color(&myGfx, WHITE, BLACK);

    gfx_printf(&myGfx, " %d", (int)atten);

    float tdiv = 16 * osc->samp_per;

    if (ui_selector == UI_TDIV)
        gfx_set_text_color(&myGfx, BLACK, WHITE);
    else
        gfx_set_text_color(&myGfx, WHITE, BLACK);

    gfx_set_cursor(&myGfx, 66, 35);
    if (tdiv < 100)
    {
        gfx_print_float(&myGfx, 12 * osc->samp_per);
        gfx_print_string(&myGfx, "us/d");
    }
    else
    {
        gfx_print_float(&myGfx, 16 * osc->samp_per / 1000.0f);
        gfx_print_string(&myGfx, "ms/d");
    }

    if (ui_selector == UI_TRIGLEV)
    {
        gfx_set_text_color(&myGfx, BLACK, WHITE);
        float vtrig = volts_from_adc(osc->trig_level);
        uint16_t h = (PIXDIV * YDIV / 2 - 1) - (vtrig * PIXDIV / availableVoltDiv[osc->vdiv_sel]);
        gfx_draw_fast_h_line(&myGfx, 1, h, 63, WHITE);
    }
    else
        gfx_set_text_color(&myGfx, WHITE, BLACK);

    gfx_set_cursor(&myGfx, 66, 43);
    gfx_print_float(&myGfx, volts_from_adc(osc->trig_level));
    gfx_print_string(&myGfx, "Vtr");

    if (ui_selector == UI_TRIGSLOPE)
        gfx_set_text_color(&myGfx, BLACK, WHITE);
    else
        gfx_set_text_color(&myGfx, WHITE, BLACK);

    if (osc->trig_slope == TRIG_RISING)
        gfx_print_string(&myGfx, " R");
    else
        gfx_print_string(&myGfx, " F");

    if (ui_selector == UI_RUNMODE)
        gfx_set_text_color(&myGfx, BLACK, WHITE);
    else
        gfx_set_text_color(&myGfx, WHITE, BLACK);
    gfx_set_cursor(&myGfx, 66, 51);

    switch (osc->runmode)
    {
    case RUN_AUTO:
        gfx_print_string(&myGfx, "Auto");
        break;
    case RUN_NORM:
        gfx_print_string(&myGfx, "Norm");
    default:
        break;
    }

    gfx_set_text_color(&myGfx, WHITE, BLACK);
    if (osc->trig_state == TRIG_FIRST || osc->trig_state == TRIG_SECOND)
        gfx_print_string(&myGfx, " Tr'd");
    else if (osc->runmode == RUN_NORM)
        gfx_print_string(&myGfx, " Wait");

    gfx_flush(&myGfx);
    gfx_set_cursor(&myGfx, 0, 0);
    gfx_clear(&myGfx);
}

// Calculates frequency in captured buffer
float measure_frequency(uint16_t *buffer, uint16_t trigger_level, float sample_period_us)
{
    int crossings = 0;
    float time_interval_sum = 0;
    int last_crossing_index = -1;

    // Detect trigger level crossings
    for (int i = 1; i < DISP_BUF_LEN; i++)
        if ((buffer[i - 1] < trigger_level && buffer[i] >= trigger_level) || (buffer[i - 1] > trigger_level && buffer[i] <= trigger_level))
        {
            if (last_crossing_index != -1)
            {
                // Time interval between crossings in microseconds
                float time_interval = (i - last_crossing_index) * sample_period_us;
                time_interval_sum += time_interval;
                crossings++;
            }
            last_crossing_index = i;
        }

    // If no crossings are found
    if (crossings == 0)
        return -1.0f; // Signal not detected

    // Calculate the average time interval between crossings
    float avg_time_interval = time_interval_sum / crossings;

    // The period is twice the average crossing interval (in microseconds)
    float period_us = 2.0f * avg_time_interval;

    // Frequency is the inverse of the period (in Hz)
    float frequency = 1.0f / (period_us * 1e-6f);

    return frequency;
}

void boot_splash()
{
    gfx_clear(&myGfx);
    gfx_set_text_color(&myGfx, BLACK, WHITE);
    gfx_print_string(&myGfx, "    O'silly'scope    \n\n");
    gfx_set_text_color(&myGfx, WHITE, BLACK);
    gfx_print_string(&myGfx, "Firmware built:\n");
    gfx_print_string(&myGfx, __DATE__);
    gfx_flush(&myGfx);
    Delay_Ms(2500);
}
