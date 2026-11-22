#ifndef OSKERNEL_H_
#define OSKERNEL_H_


#include <stdint.h>
#include <stdbool.h>


#define TASK_STACK_SIZE    		(512U)
#define IDLE_STACK_SIZE    		(64U)
#define MAX_TASKS     			(5)
#define IDLE_TASK_ID            (0)

#define SYS_CLK                 (16000000U)
#define TICK_HZ                 (1000U)
#define OS_QUANTA               (10U)

#define TASK_STATE_READY        (0U)
#define TASK_STATE_WAITING      (1U)
#define TASK_STATE_BLOCKED      (2U)
#define TASK_STATE_TERMINATED   (3U)


typedef uint8_t osKernelSem_t;

typedef struct {
    uint8_t *buffer;
    osKernelSem_t itemSem;
    osKernelSem_t spaceSem;
    uint8_t setIdx;
    uint8_t getIdx;
    uint8_t count;
    uint8_t itemSize;
}osKernelFIFO_t;

void osKernelInit(uint32_t quantaMs);
void osKernelAddTask(void (*taskHandler)(void));
void osKernelLaunch(void);
void osKernelTaskDelay(uint32_t tickCount);

void osKernelSemInit(osKernelSem_t* semaphore, uint8_t val);
void osKernelSemWait(osKernelSem_t* semaphore);
void osKernelSemGive(osKernelSem_t* semaphore);

bool osKernelFifoInit(osKernelFIFO_t* fifo, void* buffer, uint8_t itemSize, uint8_t count);
void osKernelFifoPush(osKernelFIFO_t* fifo, const void* item);
void osKernelFifoPop(osKernelFIFO_t* fifo, void* item);


#endif /* OSKERNEL_H_ */
