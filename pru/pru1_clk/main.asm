;*****************************************************************************
;
;	Author: David Youn, dty003@bucknell.edu
;
;	File: main.asm
;
;	Description: Code for PRU1 to drive ADC with CLK signal.
;
;*****************************************************************************

;*****************************************************************************
; Define constants and initial set up
;*****************************************************************************

; Define physical constants
DELAY		.set	1

; Define IO registers
CLK_MASK	.set	(1<<0)		; P8.27 - r30.t8
PPS_MASK	.set	(1<<2)		; P8.28 - r31.t10

; Define addresses
COUNTER_ADR	.set	0x00000000

; Define general purpose registers
REG_CNT_ADR	.set	r0
REG_DELAY	.set	r1
REG_PPS_NOW	.set	r2
REG_PPS_OLD .set	r3
REG_COUNTER	.set	r4

;*****************************************************************************
;                                  Main Loop
;*****************************************************************************
	.sect	".text:main"
	.clink
	.global	||main||

||main||:

	; Reset registers and load constants
	ZERO	&r0, 20
	CLR		r30.b1, r30.b1, CLK_MASK			; clear clk
	LDI		REG_CNT_ADR, COUNTER_ADR
	; Load PPS
	AND		REG_PPS_NOW.b0, r31.b1, PPS_MASK
	MOV		REG_PPS_OLD, REG_PPS_NOW

; Loop forever
TRUE:

	AND		REG_PPS_NOW.b0, r31.b1, PPS_MASK	; update pps

	XOR		r30.b1, r30.b1, CLK_MASK			; toggle clock
	ADD		REG_COUNTER, REG_COUNTER, 1			; increment counter

	; check for rising edge of pps
	QBEQ	WAIT_1, REG_PPS_NOW, REG_PPS_OLD
	QBNE	WAIT_2, REG_PPS_NOW, PPS_MASK
	SBBO	&REG_COUNTER, REG_CNT_ADR, 0, 4
	ZERO	&REG_COUNTER, 4
	QBA		CONTINUE

WAIT_1:
	QBA		WAIT_2
WAIT_2:
	QBA		WAIT_3
WAIT_3:
	QBA		WAIT_4
WAIT_4:
	QBA		CONTINUE

CONTINUE: 	; load delay
	LDI		REG_DELAY, DELAY

DELAY_LOOP:	; loop until delay is finished
	SUB		REG_DELAY, REG_DELAY, 1
	QBNE	DELAY_LOOP, REG_DELAY, 0

	MOV		REG_PPS_OLD, REG_PPS_NOW	; update pps
	QBA		TRUE						; repeat loop

END:
	; we'll never reach here
	HALT

;*****************************************************************************
;                                     END
;*****************************************************************************
