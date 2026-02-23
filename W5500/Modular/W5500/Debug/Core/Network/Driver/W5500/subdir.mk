################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Network/Driver/W5500/w5500_hw.c \
../Core/Network/Driver/W5500/w5500_ll.c 

OBJS += \
./Core/Network/Driver/W5500/w5500_hw.o \
./Core/Network/Driver/W5500/w5500_ll.o 

C_DEPS += \
./Core/Network/Driver/W5500/w5500_hw.d \
./Core/Network/Driver/W5500/w5500_ll.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Network/Driver/W5500/%.o Core/Network/Driver/W5500/%.su Core/Network/Driver/W5500/%.cyclo: ../Core/Network/Driver/W5500/%.c Core/Network/Driver/W5500/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG '-DCMSIS_device_header=<stm32g0xx.h>' -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../Core/Network -I../Core/Network/Driver/W5500 -I../Core/Network/Services -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Network-2f-Driver-2f-W5500

clean-Core-2f-Network-2f-Driver-2f-W5500:
	-$(RM) ./Core/Network/Driver/W5500/w5500_hw.cyclo ./Core/Network/Driver/W5500/w5500_hw.d ./Core/Network/Driver/W5500/w5500_hw.o ./Core/Network/Driver/W5500/w5500_hw.su ./Core/Network/Driver/W5500/w5500_ll.cyclo ./Core/Network/Driver/W5500/w5500_ll.d ./Core/Network/Driver/W5500/w5500_ll.o ./Core/Network/Driver/W5500/w5500_ll.su

.PHONY: clean-Core-2f-Network-2f-Driver-2f-W5500

