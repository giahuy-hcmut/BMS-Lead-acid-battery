################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/BMS_ADC.c \
../Core/Src/BMS_CAN.c \
../Core/Src/Scheduler.c \
../Core/Src/Shared_Data.c \
../Core/Src/Task_CAN.c \
../Core/Src/Task_Sleep.c \
../Core/Src/Task_Temperature.c \
../Core/Src/Task_Voltage.c \
../Core/Src/ds18b20.c \
../Core/Src/main.c \
../Core/Src/stm32f1xx_hal_msp.c \
../Core/Src/stm32f1xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f1xx.c 

OBJS += \
./Core/Src/BMS_ADC.o \
./Core/Src/BMS_CAN.o \
./Core/Src/Scheduler.o \
./Core/Src/Shared_Data.o \
./Core/Src/Task_CAN.o \
./Core/Src/Task_Sleep.o \
./Core/Src/Task_Temperature.o \
./Core/Src/Task_Voltage.o \
./Core/Src/ds18b20.o \
./Core/Src/main.o \
./Core/Src/stm32f1xx_hal_msp.o \
./Core/Src/stm32f1xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f1xx.o 

C_DEPS += \
./Core/Src/BMS_ADC.d \
./Core/Src/BMS_CAN.d \
./Core/Src/Scheduler.d \
./Core/Src/Shared_Data.d \
./Core/Src/Task_CAN.d \
./Core/Src/Task_Sleep.d \
./Core/Src/Task_Temperature.d \
./Core/Src/Task_Voltage.d \
./Core/Src/ds18b20.d \
./Core/Src/main.d \
./Core/Src/stm32f1xx_hal_msp.d \
./Core/Src/stm32f1xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f1xx.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/BMS_ADC.cyclo ./Core/Src/BMS_ADC.d ./Core/Src/BMS_ADC.o ./Core/Src/BMS_ADC.su ./Core/Src/BMS_CAN.cyclo ./Core/Src/BMS_CAN.d ./Core/Src/BMS_CAN.o ./Core/Src/BMS_CAN.su ./Core/Src/Scheduler.cyclo ./Core/Src/Scheduler.d ./Core/Src/Scheduler.o ./Core/Src/Scheduler.su ./Core/Src/Shared_Data.cyclo ./Core/Src/Shared_Data.d ./Core/Src/Shared_Data.o ./Core/Src/Shared_Data.su ./Core/Src/Task_CAN.cyclo ./Core/Src/Task_CAN.d ./Core/Src/Task_CAN.o ./Core/Src/Task_CAN.su ./Core/Src/Task_Sleep.cyclo ./Core/Src/Task_Sleep.d ./Core/Src/Task_Sleep.o ./Core/Src/Task_Sleep.su ./Core/Src/Task_Temperature.cyclo ./Core/Src/Task_Temperature.d ./Core/Src/Task_Temperature.o ./Core/Src/Task_Temperature.su ./Core/Src/Task_Voltage.cyclo ./Core/Src/Task_Voltage.d ./Core/Src/Task_Voltage.o ./Core/Src/Task_Voltage.su ./Core/Src/ds18b20.cyclo ./Core/Src/ds18b20.d ./Core/Src/ds18b20.o ./Core/Src/ds18b20.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/stm32f1xx_hal_msp.cyclo ./Core/Src/stm32f1xx_hal_msp.d ./Core/Src/stm32f1xx_hal_msp.o ./Core/Src/stm32f1xx_hal_msp.su ./Core/Src/stm32f1xx_it.cyclo ./Core/Src/stm32f1xx_it.d ./Core/Src/stm32f1xx_it.o ./Core/Src/stm32f1xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f1xx.cyclo ./Core/Src/system_stm32f1xx.d ./Core/Src/system_stm32f1xx.o ./Core/Src/system_stm32f1xx.su

.PHONY: clean-Core-2f-Src

