################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/accel.c \
../Src/button.c \
../Src/compass.c \
../Src/discovery.c \
../Src/gpio.c \
../Src/led.c \
../Src/led_timer.c \
../Src/main.c \
../Src/serial.c 

OBJS += \
./Src/accel.o \
./Src/button.o \
./Src/compass.o \
./Src/discovery.o \
./Src/gpio.o \
./Src/led.o \
./Src/led_timer.o \
./Src/main.o \
./Src/serial.o 

C_DEPS += \
./Src/accel.d \
./Src/button.d \
./Src/compass.d \
./Src/discovery.d \
./Src/gpio.d \
./Src/led.d \
./Src/led_timer.d \
./Src/main.d \
./Src/serial.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32F303VCTx -DSTM32 -DSTM32F3 -c -I../Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/accel.cyclo ./Src/accel.d ./Src/accel.o ./Src/accel.su ./Src/button.cyclo ./Src/button.d ./Src/button.o ./Src/button.su ./Src/compass.cyclo ./Src/compass.d ./Src/compass.o ./Src/compass.su ./Src/discovery.cyclo ./Src/discovery.d ./Src/discovery.o ./Src/discovery.su ./Src/gpio.cyclo ./Src/gpio.d ./Src/gpio.o ./Src/gpio.su ./Src/led.cyclo ./Src/led.d ./Src/led.o ./Src/led.su ./Src/led_timer.cyclo ./Src/led_timer.d ./Src/led_timer.o ./Src/led_timer.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/serial.cyclo ./Src/serial.d ./Src/serial.o ./Src/serial.su

.PHONY: clean-Src

