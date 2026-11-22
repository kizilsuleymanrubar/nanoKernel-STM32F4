#include <string.h>
#include "osKernel.h"
#include "stm32f411xe.h"

/* Idle Task */
extern void IdleTask(void);


/* Task Control Block (TCB) */
typedef struct {
    volatile uint32_t *pStack;      	/* Current Process Stack Pointer (PSP) of the task */
    volatile uint32_t delayTicks;   	/* Remaining sleep duration in system ticks */
    volatile uint8_t  state;        	/* Current task state */
    osKernelSem_t * volatile waitingOn; /* Semaphore the task waits on (valid while WAITING) */
    void (*taskHandler)(void);      	/* Task entry point function */
} TCB_t;

/* Kernel Control Structure */
typedef struct {
	TCB_t tasks[MAX_TASKS];
	volatile uint32_t sysTickPeriod;
	volatile uint8_t currentTaskId;
	volatile uint8_t taskCount;
}Kernel_t;


static uint32_t osIdleStack[IDLE_STACK_SIZE] __attribute__((aligned(8)));
static uint32_t osTaskStacks[MAX_TASKS - 1][TASK_STACK_SIZE] __attribute__((aligned(8)));
static Kernel_t osKernel;


void osKernelInit(uint32_t quantaMs) {
	SysTick->CTRL = 0;

	osKernel.sysTickPeriod = (SYS_CLK / TICK_HZ) *quantaMs - 1;
	osKernel.currentTaskId = IDLE_TASK_ID;
	osKernel.taskCount = 0;

	/* PendSV must have the lowest priority to avoid preempting hardware ISRs */
	NVIC_SetPriorityGrouping(3);
	NVIC_SetPriority(SysTick_IRQn, 14);
	NVIC_SetPriority(PendSV_IRQn, 15);

	osKernelAddTask(IdleTask);
}


/* Pends the PendSV exception to request a context switch */
static inline void osKernelTaskYield(void) {
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}


static void osKernelTaskExit(void) {
	__disable_irq();
	osKernel.tasks[osKernel.currentTaskId].state = TASK_STATE_TERMINATED;
	osKernelTaskYield();
	__enable_irq();

	while(1);
}

void osKernelAddTask(void(*taskHandler)(void)) {
	if (osKernel.taskCount >= MAX_TASKS)
	    return;

	uint8_t id = osKernel.taskCount;

	 /* Start from the highest address for a full-descending stack */
	uint32_t volatile* pPSP;

	if(id == IDLE_TASK_ID)
		pPSP = &osIdleStack[IDLE_STACK_SIZE - 1];
	else
		pPSP = &osTaskStacks[id - 1][TASK_STACK_SIZE - 1];

	/* Initial dummy stack frame for hardware exception return */
	*(pPSP) = 0X01000000;							   /* XPSR */
	*(--pPSP) = (uint32_t)taskHandler;				   /*  PC  */
	*(--pPSP) = (uint32_t)osKernelTaskExit;			   /*  LR  */

	/* Initial values for R12, R3-R0, and R11-R4 */
	for(int j = 0; j < 13 ; ++j)
		*(--pPSP) = 0;

	osKernel.tasks[id].pStack = pPSP;
	osKernel.tasks[id].taskHandler = taskHandler;
	osKernel.tasks[id].delayTicks = 0;
	osKernel.tasks[id].waitingOn = 0;
	osKernel.tasks[id].state = TASK_STATE_READY;

	/* Publish the task last: the scheduler only scans fully initialized TCBs */
	osKernel.taskCount = id + 1;
}


/* Switches from MSP to PSP and launches the first task (IdleTask) */
__attribute__ ((naked))static void osKernelSTART(uint32_t psp) {
	__asm volatile("MSR PSP, R0");
	__asm volatile("MOV R0,#0x2");
	__asm volatile("MSR CONTROL,R0");
	__asm volatile("ISB");

	__asm volatile("POP {R0-R12}");
	__asm volatile("ADD  SP,SP,#4");
	__asm volatile("POP {LR}");
	__asm volatile("ADD  SP,SP,#4");

	/* Enable global interrupts */
	__asm volatile("CPSIE I");

	/* Jump to Idle Task */
	__asm volatile("BX	LR");
}


void osKernelLaunch(void) {
	/* Start execution from IdleTask (index 0) */
	osKernel.currentTaskId = IDLE_TASK_ID;
	uint32_t pspVal = (uint32_t)osKernel.tasks[IDLE_TASK_ID].pStack;

	/* Disable global interrupts */
	__disable_irq();

	/* Enable SysTick counter */
	SysTick->VAL = 0;
	SysTick->LOAD = osKernel.sysTickPeriod;
	SysTick->CTRL |= (0x7U << 0);

	/* Start Kernel */
	osKernelSTART(pspVal);
}





void osKernelTaskDelay(uint32_t tickCount) {
	/* IdleTask must never be blocked */
    if (osKernel.currentTaskId == IDLE_TASK_ID)
        return;

    /* Zero delay simply yields execution to the next ready task */
    if (tickCount == 0) {
        osKernelTaskYield();
        return;
    }

    __disable_irq();
    osKernel.tasks[osKernel.currentTaskId].delayTicks = tickCount + 1;
    osKernel.tasks[osKernel.currentTaskId].state = TASK_STATE_BLOCKED;
    osKernelTaskYield();
    __enable_irq();
}


/* Decrements delay counters for blocked tasks */
static void osKernelUpdateTasks(void) {
	for (uint32_t i = IDLE_TASK_ID + 1; i < osKernel.taskCount; ++i) {
		if (osKernel.tasks[i].state == TASK_STATE_BLOCKED) {
			if (--osKernel.tasks[i].delayTicks == 0)
				osKernel.tasks[i].state = TASK_STATE_READY;
		}
	}
}


void SysTick_Handler(void)
{
	osKernelUpdateTasks();		// Update Ticks
	osKernelTaskYield();		// Context Switching
}


__attribute__((used, noinline)) static uint32_t osKernelGetPSP(void) {
	return (uint32_t)osKernel.tasks[osKernel.currentTaskId].pStack;
}


__attribute__((used, noinline)) static void osKernelSavePSP(uint32_t psp) {
	osKernel.tasks[osKernel.currentTaskId].pStack = (volatile uint32_t*)psp;
}


/* Round-Robin scheduler to select the next READY user task */
__attribute__((used, noinline)) static void osKernelScheduler(void) {
    for (int i = 0; i < osKernel.taskCount; ++i) {
        ++osKernel.currentTaskId;
        osKernel.currentTaskId %= osKernel.taskCount;

        if (osKernel.currentTaskId == IDLE_TASK_ID)
            continue;

        if (osKernel.tasks[osKernel.currentTaskId].state == TASK_STATE_READY)
            return;
    }

    osKernel.currentTaskId = IDLE_TASK_ID;
}


__attribute__ ((naked)) void PendSV_Handler(void)
{
	__asm volatile("PUSH {R3, LR}");
	__asm volatile("MRS R0,PSP");
	__asm volatile("STMDB R0!,{R4-R11}");
	__asm volatile("BL osKernelSavePSP");

	__asm volatile("BL osKernelScheduler");
	__asm volatile("BL osKernelGetPSP");
	__asm volatile("LDMIA R0!,{R4-R11}");
	__asm volatile("MSR PSP,R0");

	__asm volatile("POP {R3, LR}");
	__asm volatile("BX LR");
}


void osKernelSemInit(osKernelSem_t* semaphore, uint8_t val) {
	*semaphore = val;
}


void osKernelSemWait(osKernelSem_t* semaphore) {
	while(1) {
		__disable_irq();

		if(*semaphore > 0) {
			(*semaphore)--;
			__enable_irq();
			break;
		}

		osKernel.tasks[osKernel.currentTaskId].waitingOn = semaphore;
		osKernel.tasks[osKernel.currentTaskId].state = TASK_STATE_WAITING;

		osKernelTaskYield();
		__enable_irq();
	}
}


void osKernelSemGive(osKernelSem_t* semaphore) {
	__disable_irq();

	(*semaphore)++;

	for (uint32_t i = IDLE_TASK_ID + 1; i < osKernel.taskCount; ++i) {
		if (osKernel.tasks[i].state == TASK_STATE_WAITING &&
		    osKernel.tasks[i].waitingOn == semaphore) {
			osKernel.tasks[i].waitingOn = 0;
			osKernel.tasks[i].state = TASK_STATE_READY;
			break;
		}
	}

	__enable_irq();
}


bool osKernelFifoInit(osKernelFIFO_t* fifo, void* buffer, uint8_t itemSize, uint8_t count) {
    if (!fifo || !buffer || itemSize == 0 || count == 0)
        return false;

    fifo->buffer   = (uint8_t*)buffer;
    fifo->itemSize = itemSize;
    fifo->count    = count;
    fifo->setIdx   = 0;
    fifo->getIdx   = 0;

    osKernelSemInit(&fifo->itemSem, 0);
    osKernelSemInit(&fifo->spaceSem, count);

    return true;
}


void osKernelFifoPush(osKernelFIFO_t* fifo, const void* item) {
    osKernelSemWait(&fifo->spaceSem);

    __disable_irq();
    memcpy(&fifo->buffer[fifo->setIdx * fifo->itemSize], item, fifo->itemSize);
    fifo->setIdx = (fifo->setIdx + 1) % fifo->count;
    __enable_irq();

    osKernelSemGive(&fifo->itemSem);
}


void osKernelFifoPop(osKernelFIFO_t* fifo, void* item) {
    osKernelSemWait(&fifo->itemSem);

    __disable_irq();
    memcpy(item, &fifo->buffer[fifo->getIdx * fifo->itemSize], fifo->itemSize);
    fifo->getIdx = (fifo->getIdx + 1) % fifo->count;
    __enable_irq();

    osKernelSemGive(&fifo->spaceSem);
}
