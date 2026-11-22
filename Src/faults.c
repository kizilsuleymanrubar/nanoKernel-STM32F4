#include <stdint.h>
#include <stdio.h>


#define HFSR_ADDRESS                (0xE000ED2C)
#define CFSR_ADDRESS                (0xE000ED28)
#define MMFAR_ADDRESS               (0xE000ED34)
#define BFAR_ADDRESS                (0xE000ED38)
#define CCR_ADDRESS                 (0xE000ED14)


typedef struct __attribute__((packed)){
	uint32_t R0;
	uint32_t R1;
	uint32_t R2;
	uint32_t R3;
	uint32_t R12;
	uint32_t LR;      // Link Register
	uint32_t PC;      // Program Counter
	uint32_t xPSR;    // Status Register
}StackFrame_t;

typedef union __attribute__((packed)){
	uint32_t word;
	struct{
		uint32_t RESERVED1:1;  // Reserved
		uint32_t VECTTBL:1;    // Error on Vector Table Reading
		uint32_t RESERVED2:28; // Reserved
		uint32_t FORCED:1;     // Escalated Fault
		uint32_t DEBUGEVT:1;   // Debug Event
	};
}HFSR_t;

typedef union __attribute__((packed)){
	uint32_t word;
	struct{
		/* MMFSR */
		uint32_t IACCVIOL:1;    // Instruction Access Violation
		uint32_t DACCVIOL:1;    // Data Access Violation
		uint32_t RESERVED1:1;   // Reserved
		uint32_t MUNSTKERR:1;   // Unstacking Error on Exception Return
		uint32_t MSTKERR:1;     // Stacking Error on Exception Entry
		uint32_t MLSPERR:1;     // FP Lazy State Preservation
		uint32_t RESERVED2:1;   // Reserved
		uint32_t MMARVALID:1;   // MMFAR Valid
		/* BFSR */
		uint32_t IBUSERR:1;     // Instruction Bus Error
		uint32_t PRECISERR:1;   // Precise Data Bus Error
		uint32_t IMPRECISERR:1; // Imprecise Data Bus Error
		uint32_t UNSTKERR:1;    // Unstacking Error on Exception Return
		uint32_t STKERR:1;      // Stacking Error on Exception Entry
		uint32_t LSPERR:1;      // FP Lazy State Preservation
		uint32_t RESERVED3:1;   // Reserved
		uint32_t BFARVALID:1;   // BFAR Valid
		/* UFSR */
		uint32_t UNDEFINSTR:1;  // Undefined Instruction
		uint32_t INVSTATE:1;    // Invalid State
		uint32_t INVPC:1;       // Invalid PC Load
		uint32_t NOCP:1;        // No Coprocessor
		uint32_t RESERVED4:4;   // Reserved Bits
		uint32_t UNALIGNED:1;   // Unaligned Access
		uint32_t DIVBYZERO:1;   // Divide by Zero
		uint32_t RESERVED5:6;   // Reserved Bits
	};
}CFSR_t;


__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile
    (
        " tst lr, #4        \n"  // Test bit 2 of EXC_RETURN (LR) to determine active stack (0 = MSP, 1 = PSP)
        " ite eq            \n"  // Prepare conditional execution (If-Then-Else) based on the zero flag
        " mrseq r0, msp     \n"  // If bit 2 is 0, load Main Stack Pointer (MSP) into R0 (1st argument)
        " mrsne r0, psp     \n"  // If bit 2 is 1, load Process Stack Pointer (PSP) into R0 (1st argument)
        " b fault_analyzer  \n"  // Branch to fault_analyzer
    );
}


static void printStackFrame(const StackFrame_t *StackFrame)
{
    printf("R0  : 0x%08x\n", (unsigned int)StackFrame->R0);
	printf("R1  : 0x%08x\n", (unsigned int)StackFrame->R1);
	printf("R2  : 0x%08x\n", (unsigned int)StackFrame->R2);
	printf("R3  : 0x%08x\n", (unsigned int)StackFrame->R3);
	printf("R12 : 0x%08x\n", (unsigned int)StackFrame->R12);
	printf("LR  : 0x%08x\n", (unsigned int)StackFrame->LR);
	printf("PC  : 0x%08x\n", (unsigned int)StackFrame->PC);
	printf("xPSR: 0x%08x\n", (unsigned int)StackFrame->xPSR);
}


static void printMMFSR(const CFSR_t* CFSR)
{
    if (CFSR->IACCVIOL)  	printf("MemManageFault: IACCVIOL\n");
	if (CFSR->DACCVIOL)		printf("MemManageFault: DACCVIOL\n");
	if (CFSR->MUNSTKERR)	printf("MemManageFault: MUNSTKERR\n");
	if (CFSR->MSTKERR)		printf("MemManageFault: MSTKERR\n");
	if (CFSR->MLSPERR)		printf("MemManageFault: MLSPERR\n");
	if (CFSR->MMARVALID)	printf("MMFAR: 0x%08x\n", (unsigned int)*(volatile uint32_t*)MMFAR_ADDRESS);
}


static void printBFSR(const CFSR_t* CFSR)
{
    if (CFSR->IBUSERR)      printf("BusFault: IBUSERR\n");
	if (CFSR->PRECISERR)	printf("BusFault: PRECISERR\n");
	if (CFSR->IMPRECISERR)	printf("BusFault: IMPRECISERR\n");
	if (CFSR->UNSTKERR)		printf("BusFault: UNSTKERR\n");
	if (CFSR->STKERR)  		printf("BusFault: STKERR\n");
	if (CFSR->LSPERR)  		printf("BusFault: LSPERR\n");
	if (CFSR->BFARVALID)	printf("BFAR : 0x%08x\n", (unsigned int)*(volatile uint32_t*)BFAR_ADDRESS);
}


static void printUFSR(const CFSR_t* CFSR)
{
    if (CFSR->UNDEFINSTR)   printf("UsageFault: UNDEFINSTR\n");
	if (CFSR->INVSTATE)     printf("UsageFault: INVSTATE\n");
	if (CFSR->INVPC)        printf("UsageFault: INVPC\n");
	if (CFSR->NOCP)    		printf("UsageFault: NOCP\n");
	if (CFSR->UNALIGNED)    printf("UsageFault: UNALIGNED\n");
	if (CFSR->DIVBYZERO)	printf("UsageFault: DIVBYZERO\n");
}


void fault_analyzer_init(void)
{
	volatile uint32_t *pCCR = (volatile uint32_t *)CCR_ADDRESS;
	*pCCR |= (1U << 4);		// DIVBYZERO Enabled
}


void fault_analyzer(void* stackframe)
{
	HFSR_t HFSR;
	CFSR_t CFSR;
	HFSR.word = *(volatile uint32_t*)HFSR_ADDRESS;
	CFSR.word = *(volatile uint32_t*)CFSR_ADDRESS;

    if(HFSR.FORCED) {
        printf("HardFault: FORCED\n");

        if(CFSR.word & 0x000000FF) printMMFSR(&CFSR);
        if(CFSR.word & 0x0000FF00) printBFSR(&CFSR);
        if(CFSR.word & 0xFFFF0000) printUFSR(&CFSR);

        if(CFSR.word == 0)
            printf("Undefined Error\n");
    }

    if(HFSR.VECTTBL) {
		printf("HardFault: VECTTBL\n");
	}

    if(HFSR.DEBUGEVT) {
        printf("HardFault: DEBUGEVT\n");
	}

	if(stackframe)
    	printStackFrame((const StackFrame_t*)stackframe);

    while(1);
}
