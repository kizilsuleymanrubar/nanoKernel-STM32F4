#include "stm32f411xe.h"
#include "sensor.h"

#define ADC1EN 			(1U << 8)
#define ADC_VREF_CH 	(17U)
#define ADC_TEMP_CH 	(18U)
#define ADC_TIMEOUT 	(20000U)

#define CAL_VREFINT 	(*(const volatile uint16_t *)0x1FFF7A2AUL)
#define CAL_30 			(*(const volatile uint16_t *)0x1FFF7A2CUL)  /* ADC @ 30 C  */
#define CAL_110 		(*(const volatile uint16_t *)0x1FFF7A2EUL)  /* ADC @ 110 C */


typedef struct
{
    double temp_c;
    uint32_t raw_vref;
    uint32_t raw_temp;
} Sensor_t;

static Sensor_t temp_sensor;


static bool adc_convert(uint8_t ch, uint32_t *out)
{
    ADC1->SQR3 = ch;
    ADC1->CR2 |= ADC_CR2_SWSTART;

    for (uint32_t t = ADC_TIMEOUT; t > 0; --t) {
        if (ADC1->SR & ADC_SR_EOC) {
            *out = ADC1->DR;
            return true;
        }
    }

    return false;
}


void sensor_init(void)
{
    /*Enable clock access to the ADC module*/
    RCC->APB2ENR |= ADC1EN;
    (void)RCC->APB2ENR;

    /*Power up internal temperature sensor*/
    ADC->CCR |= ADC_CCR_TSVREFE;

    /*Set conversion sequence length as 1*/
    ADC1->SQR1 = 0;

    /*Set sampling time as 480 cycles*/
    ADC1->SMPR1 |= ADC_SMPR1_SMP17;
    ADC1->SMPR1 |= ADC_SMPR1_SMP18;

    /*Enable ADC module*/
    ADC1->CR2 |= ADC_CR2_ADON;

    /*Wait for ADC and temperature sensor stabilization*/
    for (volatile uint32_t i = 0; i < 1000; i++) { }
}


bool sensor_read(double *temp_c)
{
    if (!temp_c)
        return false;

    if (!adc_convert(ADC_VREF_CH, &temp_sensor.raw_vref) || temp_sensor.raw_vref == 0)
        return false;

    if (!adc_convert(ADC_TEMP_CH, &temp_sensor.raw_temp))
        return false;

    double k = (double)temp_sensor.raw_temp * CAL_VREFINT / (double)temp_sensor.raw_vref;
    temp_sensor.temp_c = 30 + 80. * (k - CAL_30) / (CAL_110 - CAL_30);
    *temp_c = temp_sensor.temp_c;

    return true;
}
