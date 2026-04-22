################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/demo_uart.c \
../Src/main.c \
../Src/serial.c \
../Src/serial_comms.c \
../Src/syscalls.c \
../Src/sysmem.c 

OBJS += \
./Src/demo_uart.o \
./Src/main.o \
./Src/serial.o \
./Src/serial_comms.o \
./Src/syscalls.o \
./Src/sysmem.o 

C_DEPS += \
./Src/demo_uart.d \
./Src/main.d \
./Src/serial.d \
./Src/serial_comms.d \
./Src/syscalls.d \
./Src/sysmem.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32F303VCTx -DSTM32 -DSTM32F3 -DSTM32F3DISCOVERY -c -I../Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/demo_uart.cyclo ./Src/demo_uart.d ./Src/demo_uart.o ./Src/demo_uart.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/serial.cyclo ./Src/serial.d ./Src/serial.o ./Src/serial.su ./Src/serial_comms.cyclo ./Src/serial_comms.d ./Src/serial_comms.o ./Src/serial_comms.su ./Src/syscalls.cyclo ./Src/syscalls.d ./Src/syscalls.o ./Src/syscalls.su ./Src/sysmem.cyclo ./Src/sysmem.d ./Src/sysmem.o ./Src/sysmem.su

.PHONY: clean-Src

