#include "ch32v003fun.h"
#include "scope.h"

// ADC capture buffers
volatile uint16_t buffer1[BUFFER_LENGTH] = {0};
volatile uint16_t buffer2[BUFFER_LENGTH] = {0};

// DMA ready (conversion done) flag
volatile uint8_t dma_ready = 1;

// available ADC clock dividers
const uint8_t availableAdcDivs[] = {2, 2, 4, 6, 8, 12, 16, 24, 32, 64, 96, 128, 0};

// pointers to the two buffers
volatile uint16_t *writeBuffer = buffer1;
volatile uint16_t *readBuffer = buffer2;

// trigger settings and flags
volatile uint16_t trigLevel = 502;
volatile uint8_t trig = RISING;
volatile uint8_t awdg_trigged = 0;
uint8_t scope_trigged;

volatile int wf_cnt = 0;

float sampPer;
float atten = 1.0f;
float frontend_offset = 1.62f;

uint8_t tdivSel = 0;

// Sampling mode
uint8_t runmode = RUN_AUTO;

// initializes ADC and DMA at startup
void init_adc()
{
    // Start DMA clock
    RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;

    // Enable DMA IRQ
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    // ADCCLK = 24 MHz => RCC_ADCPRE = 0: divide by 2
    RCC->CFGR0 &= ~(0x1F << 11);

    // Enable GPIOC and set C4 as analog input
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
    AN_IN_GPIO->CFGLR &= ~(0xf << (4 * AN_IN_PIN)); // CNF = 00: Analog, MODE = 00: Input

    // Enable the ADC1 peripheral clock
    RCC->APB2PCENR |= RCC_APB2Periph_ADC1;

    // Reset the ADC to init all regs
    RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
    RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

    // Set sequencer to channel 2 only
    ADC1->RSQR3 = AN_IN_CH;

    // Possible times: 0->3,1->9,2->15,3->30,4->43,5->57,6->73,7->241 cycles
    ADC1->SAMPTR2 = 1 /*9 cycles*/ << (3 /*offset per channel*/ * AN_IN_CH /*channel*/);

    // turn on ADC
    ADC1->CTLR2 |= ADC_ADON;

    // Reset calibration
    ADC1->CTLR2 |= ADC_RSTCAL;
    while (ADC1->CTLR2 & ADC_RSTCAL)
        ;

    // Calibrate
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL)
        ;

    // enable analog watchdog for single regular channel and interrupt
    ADC1->CTLR1 |= ADC_AWDSGL | ADC_AWDEN | ADC_AWDIE;

    // set analog watchdog channel
    ADC1->CTLR1 |= AN_IN_CH;

    // Enable continuous conversion and DMA
    ADC1->CTLR2 |= ADC_CONT | ADC_DMA | ADC_EXTSEL;

    // clear analog watchdog flag
    ADC1->STATR = ~ADC_FLAG_AWD;

    // enable ADC interrupt
    NVIC_EnableIRQ(ADC_IRQn);

    // start conversion
    ADC1->CTLR2 |= ADC_SWSTART;

    adc_set_div(availableAdcDivs[tdivSel]);
}

// Interrupt handler for the DMA, fires at the end of each buffer capture
void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt));
void DMA1_Channel1_IRQHandler()
{
    if (DMA1->INTFR & DMA1_FLAG_TC1)
    {
        DMA1->INTFCR = DMA_CTCIF1; // clear Transfer Complete interrupt
        wf_cnt++;
        dma_ready = 1; // raise ready flag (conversion finished)
    }
}

volatile uint8_t trig_sm = 0;

// Interrupt handler for the ADC analog watchdog, used for triggering
void ADC1_IRQHandler(void) __attribute__((interrupt));
void ADC1_IRQHandler()
{
    if (ADC1->STATR & ADC_FLAG_AWD)
    {
        if (trig_sm == 0)
        {
            trig_sm = 1;

            if (trig == FALLING)
            {
                ADC1->WDLTR = 0;
                ADC1->WDHTR = trigLevel;
            }
            else
            {
                ADC1->WDLTR = trigLevel;
                ADC1->WDHTR = 1023;
            }
            ADC1->STATR = ~ADC_FLAG_AWD; // clear the watchdog flag
        }
        else if (trig_sm == 1)
        {
            ADC1->CTLR1 &= ~ADC_AWDIE & ~ADC_AWDEN; // disable the watchdog and watchdog interrupt
            awdg_trigged = 1;
            trig_sm = 2;

            // start capture
            run_dma();
        }
    }
}

// arms the DMA to initiate waveform capture from ADC
void run_dma()
{
    // Swap the buffer pointers around
    dma_ready = 0;
    void *p = readBuffer;
    readBuffer = writeBuffer;
    writeBuffer = p;

    // Setup DMA Channel 1 (ADC triggered) as reading, 16-bit, linear buffer
    DMA1_Channel1->CFGR =
        DMA_DIR_PeripheralSRC | DMA_MemoryInc_Enable | DMA_PeripheralDataSize_HalfWord | DMA_MemoryDataSize_HalfWord;

    // Number of samples to get before irq
    DMA1_Channel1->CNTR = BUFFER_LENGTH;

    // Source
    DMA1_Channel1->PADDR = (uint32_t)&ADC1->RDATAR;

    // Destination
    DMA1_Channel1->MADDR = (uint32_t)writeBuffer;

    // Enable DMA channel
    DMA1_Channel1->CFGR |= DMA_CFGR1_EN | DMA_IT_TC;
}

// Set the ADC clock divider
void adc_set_div(uint8_t div)
{
    sampPer = 20.0f * (1 / 48.0f) * div;

    // hack to zoom in onto the waveform at the fastest sample rate (extend time base down to 5us/d)
    if (tdivSel == 0)
        sampPer = sampPer / 2;

    RCC->CFGR0 &= ~((uint32_t)(0b11111) << 11);
    switch (div)
    {
    case 2:
        break;
    case 4:
        RCC->CFGR0 |= RCC_ADCPRE_DIV4;
        break;
    case 6:
        RCC->CFGR0 |= RCC_ADCPRE_DIV6;
        break;
    case 8:
        RCC->CFGR0 |= RCC_ADCPRE_DIV8;
        break;
    case 12:
        RCC->CFGR0 |= 0xA000;
        break;
    case 16:
        RCC->CFGR0 |= 0xE000;
        break;
    case 24:
        RCC->CFGR0 |= 0xA800;
        break;
    case 32:
        RCC->CFGR0 |= 0xE800;
        break;
    case 64:
        RCC->CFGR0 |= 0xF000;
        break;
    case 96:
        RCC->CFGR0 |= 0xB800;
        break;
    case 128:
        RCC->CFGR0 |= 0xF800;
        break;
    default:
        break;
    }
}

// Capture one waveform
void capture_waveform()
{
    if (dma_ready)
    {
        scope_trigged = 0;

        static uint16_t prevMs;
        uint16_t currentMs = SysTick->CNT / DELAY_MS_TIME;
        if (currentMs < prevMs)
            prevMs = currentMs;

        if (runmode == RUN_AUTO && currentMs - prevMs > 150 && !awdg_trigged)
        {
            run_dma();
            prevMs = currentMs;
        }
        else
        {
            if (awdg_trigged)
                scope_trigged = 1; // mark triggered

            // restart capture
            // set thresholds for triggering
            // FRONTEND INVERTS SIGNAL, TRIG SLOPES ARE OPPOSITE FROM USUAL
            if (trig == FALLING)
            {
                // falling edge: arm when we're below the trigger level (outside of the set window)
                ADC1->WDLTR = trigLevel;
                ADC1->WDHTR = 1023;
            }
            else
            {
                // risaing edge: arm when we're above the trigger level (outside of the set window)
                ADC1->WDLTR = 0;
                ADC1->WDHTR = trigLevel;
            }
            trig_sm = 0;
            awdg_trigged = 0;                     // clear trigger flag
            ADC1->STATR = ~ADC_FLAG_AWD;          // clear analog watchdog flag
            ADC1->CTLR1 |= ADC_AWDEN | ADC_AWDIE; // enable watchdog again, for next capture
        }
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
