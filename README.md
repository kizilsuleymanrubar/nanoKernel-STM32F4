# nanoKernel-STM32F4
 
A lightweight preemptive RTOS kernel written from scratch in C and ARM Thumb-2 Assembly for **STM32F4 (Cortex-M4)**, tested on the **STM32F411**. The repository includes a demo application: a producer-consumer pipeline that reads the internal temperature sensor, passes samples through a FIFO queue and prints them over UART, plus a built-in HardFault analyzer.
 
---
 
## Key Features
 
- **Preemptive round-robin scheduler:** context switching driven by `SysTick` and `PendSV`, with a configurable time quantum.
- **PSP/MSP stack separation:** kernel and interrupts run on the Main Stack Pointer, tasks run on the Process Stack Pointer (8-byte aligned stacks).
- **Task management:** non-blocking delays (`osKernelTaskDelay`), yielding, and safe handling of tasks that return.
- **Synchronization and communication:**
  - **Counting semaphores:** `osKernelSemInit`, `osKernelSemWait`, `osKernelSemGive`, protected by short critical sections.
  - **Copy-based FIFO queue:** a circular buffer (`osKernelFIFO_t`) built on two counting semaphores (`itemSem` and `spaceSem`) for producer-consumer data transfer. The buffer is provided by the caller (static, no heap needed).
- **Low-power idle task:** `IdleTask` executes `WFI` when no other task is ready.
- **HardFault analyzer:** a naked `HardFault_Handler` detects the active stack pointer (MSP or PSP), dumps `R0-R3`, `R12`, `LR`, `PC`, `xPSR`, and decodes `HFSR`, `CFSR` (`MMFSR`, `BFSR`, `UFSR`), `MMFAR` and `BFAR` over UART.
---
 
## Repository Layout
 
```
├── CMSIS/              # ARM / ST CMSIS headers (not authored here)
├── Inc/
│   ├── osKernel.h      # Kernel API, TCB definition, configuration macros
│   ├── faults.h        # HardFault analyzer interface
│   ├── led.h           # On-board LED driver (PA5)
│   ├── sensor.h        # Internal temperature sensor (ADC1) driver
│   └── uart.h          # USART2 driver and printf retargeting
├── Src/
│   ├── main.c          # Entry point and Producer-Consumer demo tasks
│   ├── osKernel.c      # Scheduler, SysTick/PendSV handlers, semaphores, FIFO
│   ├── faults.c        # Naked HardFault handler and register decoding
│   ├── led.c           # Bare-metal GPIO driver
│   ├── sensor.c        # ADC1 calibration and temperature calculation
│   └── uart.c          # Bare-metal USART2 driver (115200 8N1)
└── README.md
```
 
---
 
## Kernel Configuration
 
Tunable in `osKernel.h`:
 
| Macro             | Default        | Description                                      |
| ----------------- | -------------- | ------------------------------------------------ |
| `TASK_STACK_SIZE` | `512U` (words) | Stack per user task (2 KB)                       |
| `IDLE_STACK_SIZE` | `64U` (words)  | Stack for the idle task (256 B)                  |
| `MAX_TASKS`       | `5`            | Maximum number of tasks, including `IdleTask`    |
| `SYS_CLK`         | `16000000U`    | System clock in Hz (16 MHz HSI)                  |
| `TICK_HZ`         | `1000U`        | Tick rate (1 ms)                                 |
| `OS_QUANTA`       | `10U`          | Time slice per task, in ticks (ms)               |
 
---
 
## Demo Application
 
`main.c` runs four tasks:
 
| Task           | ID | Behaviour                                                                                                   |
| -------------- | -- | ----------------------------------------------------------------------------------------------------------- |
| `IdleTask`     | 0  | Executes `WFI` when every other task is blocked or waiting.                                                 |
| `LedTask`      | 1  | Toggles the user LED (`PA5`) every 100 delay units (100 × `OS_QUANTA` = 1 s) as a heartbeat.                                                |
| `ProducerTask` | 2  | Reads the internal temperature sensor (ADC1) every 50 delay units (500 ms) and pushes the value into the FIFO.             |
| `ConsumerTask` | 3  | Blocks on the FIFO until data arrives, pops the value and prints it over USART2 (`PA2`/`PA3`, 115200 baud). |
 
### Temperature measurement
 
The sensor driver uses ADC1 channel 18 (temperature sensor) and channel 17 (`VREFINT`) together with the factory calibration values stored in system memory:
 
| Value         | Address        |
| ------------- | -------------- |
| `VREFINT_CAL` | `0x1FFF7A2A`   |
| `TS_CAL1`     | `0x1FFF7A2C`   |
| `TS_CAL2`     | `0x1FFF7A2E`   |
 
### Sample serial output
 
```
Temperature = 27.45 C
Temperature = 27.51 C
Temperature = 27.48 C
```
 
### Sample HardFault dump
 
```
HardFault: FORCED
UsageFault: DIVBYZERO
R0  : 0x0000000a
R1  : 0x00000000
R2  : 0x20000410
R3  : 0x08001234
R12 : 0x00000000
LR  : 0x080008a5
PC  : 0x080008b2
xPSR: 0x61000000
```
 
> **Note:** on Cortex-M4, divide-by-zero and unaligned-access traps are disabled by default. Set `DIV_0_TRP` / `UNALIGN_TRP` in `SCB->CCR` if you want those conditions to raise a fault.
 
After printing the dump, the handler halts in an infinite loop so the state can be inspected with a debugger.
 
---
 
## Limitations
 
This is a teaching kernel, not a production RTOS. Currently:
 
- No task priorities (pure round-robin).
- No mutexes or priority inheritance.
- No stack overflow detection.
- Tasks are statically allocated (`MAX_TASKS`); no dynamic task creation or deletion.
- No hardware FPU support.
 
