#include "ch32fun.h"
#include "scope.h"
#include "adc.h"

// ADC capture buffers
#define CIRC_BUF_LEN (2 * BUFFER_LENGTH)

volatile uint16_t disp_buf[BUFFER_LENGTH] = {0};
volatile uint16_t circ_buf[CIRC_BUF_LEN] = {0}; // circular

// DMA ready (conversion done) flag
volatile uint8_t dma_ready = 1;
volatile uint8_t dma_halves_filled = 0;

// pointers to the two buffers
volatile uint16_t *readBuffer = disp_buf;

// trigger settings and flags
volatile uint16_t trigLevel = 502;
volatile uint8_t trig = RISING;
uint8_t scope_trigged;

volatile int wf_cnt = 0;

float sampPer;
float atten = 1.0f;
float frontend_offset = 1.62f;

uint8_t tdivSel = 1;

// Sampling mode
uint8_t runmode = RUN_AUTO;

volatile uint8_t trig_sm = 0;

enum
{
    NO_TRIGGER = 0,
    TRIG_FIRST,
    TRIG_SECOND,
    POST_TRIG,
    TRIG_DISABLED
};

volatile uint8_t trig_state = NO_TRIGGER;
volatile uint8_t trig_area = NO_TRIGGER;

// DMA interrupt handler
void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt));
void DMA1_Channel1_IRQHandler()
{
    if (dma_halves_filled < 3)
        dma_halves_filled++;

    if (DMA1->INTFR & DMA1_FLAG_HT1)
    {
        if (trig_state == NO_TRIGGER && awdg_trigged)
        {
            trig_state = TRIG_FIRST;
            trig_area = trig_state;
            DMA1_Channel1->CFGR &= ~DMA_CFGR1_EN;
            trig_state = POST_TRIG;
            dma_ready = 1;
        }
        DMA1->INTFCR = DMA_HTIF1; // clear Half Transfer interrupt
    }

    if (DMA1->INTFR & DMA1_FLAG_TC1)
    {
        if (trig_state == NO_TRIGGER && awdg_trigged)
        {
            trig_state = TRIG_SECOND;
            trig_area = trig_state;

            DMA1_Channel1->CFGR &= ~DMA_CFGR1_EN;
            trig_state = POST_TRIG;
            dma_ready = 1;
        }

        else if (trig_state == TRIG_DISABLED)
            dma_ready = 1;

        DMA1->INTFCR = DMA_CTCIF1; // clear Transfer Complete interrupt
    }
}

void start_circular_capture(uint16_t *buf, uint32_t cnt)
{
    dma_halves_filled = 0;
    dma_ready = 0;

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

void start_linear_capture(uint16_t *buf, uint32_t cnt)
{
    dma_ready = 0;
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

void merge_buffers()
{
    if (trig_area == TRIG_FIRST)
    {
        scope_trigged = 1;

        // find trigger point in first area
        int trig_index = find_trigger(circ_buf, CIRC_BUF_LEN / 2, trigLevel, trig);

        int idx = trig_index;
        int wrap = 1;
        for (int i = (BUFFER_LENGTH / 2) - 1; i >= 0; i--)
            if (idx >= 0)
                disp_buf[i] = circ_buf[idx--]; // fetch from first half
            else
                disp_buf[i] = circ_buf[CIRC_BUF_LEN - (wrap++)]; // then wraparound second half

        idx = trig_index;
        for (int i = BUFFER_LENGTH / 2; i < BUFFER_LENGTH && idx < CIRC_BUF_LEN / 2; i++)
            disp_buf[i] = circ_buf[idx++];
    }

    else if (trig_area == TRIG_SECOND)
    {
        scope_trigged = 1;

        // find trigger point in second area
        int trig_index = find_trigger(circ_buf + (CIRC_BUF_LEN / 2), CIRC_BUF_LEN / 2, trigLevel, trig) + (CIRC_BUF_LEN / 2);

        int idx = trig_index;
        for (int i = (BUFFER_LENGTH / 2) - 1; i >= 0; i--)
            disp_buf[i] = circ_buf[idx--];

        idx = trig_index;
        int wrap = 0;
        for (int i = BUFFER_LENGTH / 2; i < BUFFER_LENGTH; i++)
            if (idx < CIRC_BUF_LEN)
                disp_buf[i] = circ_buf[idx++];
            else
                disp_buf[i] = circ_buf[wrap++];
    }

    else
    {
        if (runmode == RUN_AUTO)
            for (int i = 0; i < BUFFER_LENGTH; i++)
                disp_buf[i] = circ_buf[i];
        scope_trigged = 0;
    }
}

// Capture one waveform
void capture_waveform()
{
    static uint16_t prevMs;
    uint16_t currentMs = SysTick->CNT / DELAY_MS_TIME;
    if (currentMs < prevMs)
        prevMs = currentMs;

    if (runmode == RUN_AUTO && currentMs - prevMs > 150 && trig_state == NO_TRIGGER)
    {
        DMA1_Channel1->CFGR &= ~DMA_CFGR1_EN; // stop DMA
        trig_state = TRIG_DISABLED;
        start_linear_capture(circ_buf, BUFFER_LENGTH);
        prevMs = currentMs;
    }

    if (dma_ready)
    {
        merge_buffers();
        trig_area = NO_TRIGGER;
        trig_state = NO_TRIGGER;
        start_circular_capture(circ_buf, CIRC_BUF_LEN);
        adc_arm_trigger();
    }
}

// Calculates frequency in captured buffer
float measure_frequency(uint16_t *buffer, uint16_t trigger_level, float sample_period_us)
{
    int crossings = 0;
    float time_interval_sum = 0;
    int last_crossing_index = -1;

    // Detect trigger level crossings
    for (int i = 1; i < BUFFER_LENGTH; i++)
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
