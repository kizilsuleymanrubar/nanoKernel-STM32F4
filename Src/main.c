#include "osKernel.h"
#include "faults.h"
#include "uart.h"
#include "sensor.h"
#include "led.h"


#define FIFO_SIZE	32


volatile uint32_t ledProfiler, producerProfiler, consumerProfiler;
volatile uint32_t sensorErrors;
static double fifoBuf[FIFO_SIZE];
osKernelFIFO_t FIFOx;


void IdleTask(void);
void LedTask(void);
void ProducerTask(void);
void ConsumerTask(void);


int main(void)
{
	// Initialize fault analyzer
	fault_analyzer_init();

	// Initialize UART
	uart_init();

	// Initialize Kernel
	osKernelInit(OS_QUANTA);
	osKernelAddTask(LedTask);
	osKernelAddTask(ProducerTask);
	osKernelAddTask(ConsumerTask);

	// Initialize FIFOx
	if(!osKernelFifoInit(&FIFOx, fifoBuf, sizeof(double), FIFO_SIZE))
	{
		printf("FIFO creation failed...\n");
		while(1);
	}

	// Launch Kernel
	osKernelLaunch();

    /* Loop forever */
	for(;;);
}


void IdleTask(void) {
	while(1){
		__asm volatile("WFI");
		// TODO : WFI for sleep mode
	}
}


void LedTask(void)
{
	led_init();

	while(1){
		ledProfiler++;
		led_toggle();
		osKernelTaskDelay(1000 / OS_QUANTA);
	}
}


void ProducerTask(void)
{
	sensor_init();
	double temp;

	while(1){
		if(sensor_read(&temp)) {
			osKernelFifoPush(&FIFOx, &temp);
			producerProfiler++;
		}
		else
			sensorErrors++;

		osKernelTaskDelay(500 /OS_QUANTA);
	}
}


void ConsumerTask(void)
{
	double temp;

	while(1){
		osKernelFifoPop(&FIFOx, &temp);
		consumerProfiler++;
		printf("Temperature = %.2f C\n", temp);
	}
}
