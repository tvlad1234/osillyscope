#include "ch32fun.h"

#include "scope.h"

// available ADC clock dividers
const uint8_t availableAdcDivs[] = {2, 2, 4, 6, 8, 12, 16, 24, 32, 64, 96, 128, 0};

volatile uint8_t awdg_trigged = 0;

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

    // enable analog watchdog for single regular channel
    ADC1->CTLR1 |= ADC_AWDSGL;

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

void adc_arm_trigger()
{
    if (trig == FALLING)
    {
        // falling edge: arm when we're below the trigger level (outside of the set window)
        ADC1->WDLTR = trigLevel;
        ADC1->WDHTR = 1023;
    }
    else
    {
        // rising edge: arm when we're above the trigger level (outside of the set window)
        ADC1->WDLTR = 0;
        ADC1->WDHTR = trigLevel;
    }

    trig_sm = 0;
    awdg_trigged = 0;                     // clear trigger flag
    ADC1->STATR = ~ADC_FLAG_AWD;          // clear analog watchdog flag
    ADC1->CTLR1 |= ADC_AWDEN | ADC_AWDIE; // enable watchdog again, for next capture
}

// Interrupt handler for the ADC analog watchdog, used for triggering
void ADC1_IRQHandler(void) __attribute__((interrupt));
void ADC1_IRQHandler()
{
    if (ADC1->STATR & ADC_FLAG_AWD)
    {
        if (dma_halves_filled >= 2)
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
            }
            else if (trig_sm == 1)
            {
                // TRIGGER EVENT!
                // disable the watchdog and watchdog interrupt
                ADC1->CTLR1 &= ~(ADC_AWDIE | ADC_AWDEN);
                awdg_trigged = 1;
                trig_sm = 2;
            }
        }
        ADC1->STATR &= ~ADC_FLAG_AWD; // clear the watchdog flag
    }
}
