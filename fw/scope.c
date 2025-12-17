#include "ch32fun.h"
#include "scope.h"
#include "adc.h"
#include "ui.h"

float atten = 1.0f;
float frontend_offset = 1.62f;

static oscilloscope_t *osc;

void init_oscilloscope(oscilloscope_t *oscope)
{
    osc = oscope;

    init_adc(osc);
    init_ui();

    // Splash screen
    boot_splash();
}

// DMA interrupt handler
void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt));
void DMA1_Channel1_IRQHandler()
{
    if (osc->dma_halves_filled < 3)
        osc->dma_halves_filled++;

    if (DMA1->INTFR & DMA1_FLAG_HT1)
    {
        if (osc->trig_state == NO_TRIGGER && osc->awdg_trigged)
        {
            osc->trig_state = TRIG_FIRST;
            DMA1_Channel1->CFGR &= ~DMA_CFGR1_EN;
            osc->dma_ready = 1;
        }
        DMA1->INTFCR = DMA_HTIF1; // clear Half Transfer interrupt
    }

    if (DMA1->INTFR & DMA1_FLAG_TC1)
    {
        if (osc->trig_state == NO_TRIGGER && osc->awdg_trigged)
        {
            osc->trig_state = TRIG_SECOND;
            DMA1_Channel1->CFGR &= ~DMA_CFGR1_EN;
            osc->dma_ready = 1;
        }

        else if (osc->trig_state == TRIG_DISABLED)
            osc->dma_ready = 1;

        DMA1->INTFCR = DMA_CTCIF1; // clear Transfer Complete interrupt
    }
}

void start_circular_capture(oscilloscope_t *osc, uint16_t *buf, uint32_t cnt)
{
    osc->dma_halves_filled = 0;
    osc->dma_ready = 0;

    // Setup DMA Channel 1 (ADC triggered) as reading, 16-bit, circular buffer
    DMA1_Channel1->CFGR =
        DMA_DIR_PeripheralSRC | DMA_MemoryInc_Enable | DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord | DMA_CFGR1_CIRC;

    // Number of samples to get before irq
    DMA1_Channel1->CNTR = cnt;

    // Source
    DMA1_Channel1->PADDR = (uint32_t)&ADC1->RDATAR;

    // Destination
    DMA1_Channel1->MADDR = (uint32_t)buf;

    // Enable DMA channel
    DMA1_Channel1->CFGR |= DMA_CFGR1_EN | DMA_CFGR1_HTIE | DMA_CFGR1_TCIE;
}

void start_linear_capture(oscilloscope_t *osc, uint16_t *buf, uint32_t cnt)
{
    osc->dma_ready = 0;
    // Setup DMA Channel 1 (ADC triggered) as reading, 16-bit, linear buffer
    DMA1_Channel1->CFGR =
        DMA_DIR_PeripheralSRC | DMA_MemoryInc_Enable | DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord;

    // Number of samples to get before irq
    DMA1_Channel1->CNTR = cnt;

    // Source
    DMA1_Channel1->PADDR = (uint32_t)&ADC1->RDATAR;

    // Destination
    DMA1_Channel1->MADDR = (uint32_t)buf;

    // Enable DMA channel
    DMA1_Channel1->CFGR |= DMA_CFGR1_EN | DMA_IT_TC;
}

int find_trigger(uint16_t *buf, int len, uint16_t level, int rising)
{
    for (int i = 1; i < len; i++)
        if (!rising)
        {
            if (buf[i - 1] < level && buf[i] >= level)
                return i;
        }
        else
        {
            if (buf[i - 1] > level && buf[i] <= level)
                return i;
        }
    return -1;
}

void merge_buffers(oscilloscope_t *osc)
{
    if (osc->trig_state == TRIG_FIRST)
    {
        // find trigger point in first area
        int trig_index = find_trigger(osc->adc_buf, CIRC_BUF_LEN / 2, osc->trig_level, osc->trig_slope);

        int idx = trig_index;
        int wrap = 1;
        for (int i = (DISP_BUF_LEN / 2) - 1; i >= 0; i--)
            if (idx >= 0)
                osc->wave_buf[i] = osc->adc_buf[idx--]; // fetch from first half
            else
                osc->wave_buf[i] = osc->adc_buf[CIRC_BUF_LEN - (wrap++)]; // then wraparound second half

        idx = trig_index;
        for (int i = DISP_BUF_LEN / 2; i < DISP_BUF_LEN && idx < CIRC_BUF_LEN / 2; i++)
            osc->wave_buf[i] = osc->adc_buf[idx++];
    }

    else if (osc->trig_state == TRIG_SECOND)
    {
        // find trigger point in second area
        int trig_index = find_trigger(osc->adc_buf + (CIRC_BUF_LEN / 2), CIRC_BUF_LEN / 2, osc->trig_level, osc->trig_slope) + (CIRC_BUF_LEN / 2);

        int idx = trig_index;
        for (int i = (DISP_BUF_LEN / 2) - 1; i >= 0; i--)
            osc->wave_buf[i] = osc->adc_buf[idx--];

        idx = trig_index;
        int wrap = 0;
        for (int i = DISP_BUF_LEN / 2; i < DISP_BUF_LEN; i++)
            if (idx < CIRC_BUF_LEN)
                osc->wave_buf[i] = osc->adc_buf[idx++];
            else
                osc->wave_buf[i] = osc->adc_buf[wrap++];
    }

    else
    {
        if (osc->runmode == RUN_AUTO)
            for (int i = 0; i < DISP_BUF_LEN; i++)
                osc->wave_buf[i] = osc->adc_buf[i];
    }
}

// Capture one waveform
void capture_waveform(oscilloscope_t *osc)
{
    static uint16_t prevMs;
    uint16_t currentMs = SysTick->CNT / DELAY_MS_TIME;
    if (currentMs < prevMs)
        prevMs = currentMs;

    if (osc->runmode == RUN_AUTO && currentMs - prevMs > 150 && osc->trig_state == NO_TRIGGER)
    {
        DMA1_Channel1->CFGR &= ~DMA_CFGR1_EN; // stop DMA
        osc->trig_state = TRIG_DISABLED;
        start_linear_capture(osc, osc->adc_buf, DISP_BUF_LEN);
        prevMs = currentMs;
    }

    if (osc->dma_ready)
    {
        merge_buffers(osc);
        osc->trig_state = NO_TRIGGER;
        start_circular_capture(osc, osc->adc_buf, CIRC_BUF_LEN);
        adc_arm_trigger();
    }
}
