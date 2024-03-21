################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.asm $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: PRU Compiler'
	"C:/ti/ccs1120/ccs/tools/compiler/ti-cgt-pru_2.3.3/bin/clpru" -v3 --include_path="C:/ti/ccs1120/ccs/ccs_base/pru/include" --include_path="C:/Users/daviy/OneDrive/Desktop/PRU Stuff/pru0_readCoil" --include_path="C:/ti/ccs1120/ccs/tools/compiler/ti-cgt-pru_2.3.3/include" --define=am3359 --define=pru0 -g --diag_warning=225 --diag_wrap=off --display_error_number --endian=little --hardware_mac=on --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


