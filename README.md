# nanoKernel-STM32F4

A lightweight, deterministic, preemptive Real-Time Operating System (RTOS) kernel written from scratch in C and ARM Thumb-2 Assembly for **STM32F4 (ARM Cortex-M4)** microcontrollers, tested on the **STM32F411**.

This repository also includes a complete bare-metal demonstration application featuring a **Producer-Consumer** architecture using internal temperature sensor readings, a thread-safe **FIFO queue**, **Counting Semaphores**, and a built-in **HardFault Exception Analyzer**.

---

## ✨ Key Features

* **Preemptive Round-Robin Scheduler:** Deterministic context switching driven by `SysTick` and `PendSV` exceptions with configurable time quanta.
* **PSP/MSP Stack Separation:** Isolates kernel/interrupt execution (Main Stack Pointer) from user tasks (Process Stack Pointer) with 8-byte stack alignment.
* **Task Management & Timing:** Non-blocking task delays (`osKernelTaskDelay`), cooperative yielding, and automatic task termination handling.
* **Inter-Task Synchronization & Communication:**
  * **Counting Semaphores:** Blocking `Wait` and `Give` primitives (`osKernelSemWait`, `osKernelSemGive`) with critical section protection.
  * **Thread-Safe FIFO Queue:** Generic circular buffer (`osKernelFIFO_t`) built on top of dual counting semaphores (`itemSem` & `spaceSem`) for safe Producer-Consumer data transfer.
* **Low-Power Idle Task:** Built-in `IdleTask` utilizing the ARM `WFI` (Wait For Interrupt) instruction to reduce power consumption when no user tasks are ready.
* **Hardware Fault Analyzer:** Custom `HardFault_Handler` that automatically detects the active stack pointer (`MSP` or `PSP`), dumps core CPU registers (`R0-R3`, `R12`, `LR`, `PC`, `xPSR`), and decodes `HFSR`, `CFSR` (`MMFSR`, `BFSR`, `UFSR`), `MMFAR`, and `BFAR` fault status registers over UART.

---

## 🏗️ Project Architecture

```text
├── Core/
│   ├── Inc/
│   │   ├── osKernel.h      # RTOS kernel API, TCB definitions, and configuration macros
│   │   ├── faults.h        # HardFault analyzer interface
│   │   ├── led.h           # On-board LED driver interface (PA5)
│   │   ├── sensor.h        # Internal STM32 temperature sensor ADC driver
│   │   └── uart.h          # USART2 driver & printf retargeting
│   └── Src/
│       ├── main.c          # Application entry point & Producer-Consumer tasks
│       ├── osKernel.c      # Scheduler, PendSV/SysTick handlers, Semaphores & FIFO
│       ├── faults.c        # Naked HardFault handler & register diagnostics
│       ├── led.c           # GPIO bare-metal implementation
│       ├── sensor.c        # ADC1 calibration & temperature calculation
│       └── uart.c          # Bare-metal USART2 (115200 8N1) implementation
└── README.md
```

---

## ⚙️ Kernel Configuration

Kernel parameters can be easily tuned in `osKernel.h`:

| Macro | Default Value | Description |
| :--- | :--- | :--- |
| `TASK_STACK_SIZE` | `512U` (words) | Stack size allocated per user task (2 KB) |
| `IDLE_STACK_SIZE` | `64U` (words) | Stack size allocated for the Idle task (256 B) |
| `MAX_TASKS` | `5` | Maximum concurrent tasks (including `IdleTask`) |
| `SYS_CLK` | `16000000U` | System clock frequency in Hz (16 MHz HSI) |
| `TICK_HZ` | `1000U` | System tick resolution (1 ms) |
| `OS_QUANTA` | `10U` | Time slice per task in milliseconds |

---

## 🚀 Demo Application Overview

The included `main.c` demonstrates the RTOS capabilities with four concurrent tasks:

1. **`IdleTask` (ID 0):** Executes `__WFI()` whenever all user tasks are blocked or waiting.
2. **`LedTask` (ID 1):** Toggles the onboard user LED (`PA5`) every `100` ticks as a system heartbeat indicator.
3. **`ProducerTask` (ID 2):** Reads the STM32F411 internal temperature sensor via `ADC1` (using factory calibration values at `0x1FFF7A2A`–`0x1FFF7A2E`) every `50` ticks and pushes the temperature data into `FIFOx`.
4. **`ConsumerTask` (ID 3):** Blocks on `FIFOx` until new temperature data arrives, pops the value, and transmits it via `USART2` (`PA2`/`PA3` at `115200 baud`).

### Sample Serial Output

```text
Temperature = 27.45 C
Temperature = 27.51 C
Temperature = 27.48 C
```

### Sample HardFault Diagnostic Dump

If a divide-by-zero, unaligned access, or bus fault occurs, the kernel halts safely and outputs:

```text
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

---

## 🛠️ Build & Flashing Instructions

1. **Hardware Requirements:**
   * STM32F411RE Nucleo board (or any STM32F4 target).
   * USB-to-UART connection on `PA2` (TX) and `PA3` (RX) (integrated via ST-Link VCP on Nucleo boards).
2. **Compiler Settings:**
   * Toolchain: `arm-none-eabi-gcc` (STM32CubeIDE / Makefile / CMake).
   * Target CPU: `-mcpu=cortex-m4 -mthumb -mfloat-abi=soft` *(FPU context saving is intentionally omitted for minimal context-switch latency)*.
3. **Run:**
   * Clone the repository, import the source files into your STM32F4 project, build, and flash to the target.
   * Open a serial terminal at **115200 baud, 8-N-1** to observe live sensor logs.





