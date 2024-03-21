;*****************************************************************************
;
;	Author: David Youn, dty003@bucknell.edu
;
;	File: main.asm - pru0_readCoil
;
;	Description: Code for PRU0 to drive ADC and store data to memory
;
;*****************************************************************************

;*****************************************************************************
; Define constants and initial set up
;*****************************************************************************

; Define physical constants
SCLK_PAUSE	.set	150
SCLK_DELAY	.set	50
SCLK_CYCLES	.set 	24		; 24-bit words

; Define IO registers
SCLK_MASK	.set	(1<<7)		; P8.11 - r30.t15
SCLK_BIT	.set	15
DATA_MASK	.set	(1<<7)		; P8.15 - r31.t15
DRDY_BIT	.set	15

; Define addresses
PRU0_BASE	.set	0x00000000
RDY_OFFSET	.set	0x00000001

; Define general purpose registers
REG_PRU0_BASE .set	r0
REG_RDY		 .set	r1
REG_SCLK_CNT .set	r2
REG_SCLK_DEL .set	r3
REG_WORD	 .set	r4

;*****************************************************************************
;                                  Main Loop
;*****************************************************************************
	.sect	".text:main"
	.clink
	.global	||main||

||main||:

	ZERO	&r0, 20
	LDI		REG_PRU0_BASE, PRU0_BASE

; hold SCLK high so ADC can reset until ARM says so
	SET		r30, r30, SCLK_BIT
RESET:
	LBBO	&REG_RDY, REG_PRU0_BASE, RDY_OFFSET, 1
	QBEQ	RESET, REG_RDY, 1
	CLR		r30, r30, SCLK_BIT

; loop forever
TRUE:

	; wait for DRDY line to go high, low, high
	WBS		r31, DRDY_BIT
	WBC		r31, DRDY_BIT
	WBS		r31, DRDY_BIT

; wait for DRDY to settle before starting SCLK
	LDI		REG_SCLK_DEL, SCLK_PAUSE
PAUSE:
	SUB		REG_SCLK_DEL, REG_SCLK_DEL, 1
	QBNE	PAUSE, REG_SCLK_DEL, 0

; start SCLK and start writing to REG_WORD
	LDI		REG_SCLK_CNT, SCLK_CYCLES
	ZERO	&REG_WORD, 4
SCLK_RISING:
	SET		r30, r30, SCLK_BIT
; push new bit on rising edge of SCLK
	LSL		REG_WORD, REG_WORD, 1
	QBBC	ELSE, r31, DRDY_BIT
	SET		REG_WORD, REG_WORD, 0
ELSE:

	LDI		REG_SCLK_DEL, SCLK_DELAY
DELAY_HIGH:
	SUB		REG_SCLK_DEL, REG_SCLK_DEL, 1
	QBNE	DELAY_HIGH, REG_SCLK_DEL, 0
SCLK_FALLING:
	CLR		r30, r30, SCLK_BIT
	LDI		REG_SCLK_DEL, SCLK_DELAY
DELAY_LOW:
	SUB		REG_SCLK_DEL, REG_SCLK_DEL, 1
	QBNE	DELAY_LOW, REG_SCLK_DEL, 0
; continue SCLK until all bits are read
	SUB		REG_SCLK_CNT, REG_SCLK_CNT, 1
	QBNE	SCLK_RISING, REG_SCLK_CNT, 0

	QBA		TRUE

END:
	; we'll never reach here
	HALT

;*****************************************************************************
;                                     END
;*****************************************************************************
