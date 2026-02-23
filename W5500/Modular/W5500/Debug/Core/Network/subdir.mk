################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Network/net_manager.c \
../Core/Network/net_socket.c 

OBJS += \
./Core/Network/net_manager.o \
./Core/Network/net_socket.o 

C_DEPS += \
./Core/Network/net_manager.d \
./Core/Network/net_socket.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Network/%.o Core/Network/%.su Core/Network/%.cyclo: ../Core/Network/%.c Core/Network/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG '-DCMSIS_device_header=<stm32g0xx.h>' -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../Core/Network -I../Core/Network/Driver/W5500 -I../Core/Network/Services -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Network

clean-Core-2f-Network:
	-$(RM) ./Core/Network/net_manager.cyclo ./Core/Network/net_manager.d ./Core/Network/net_manager.o ./Core/Network/net_manager.su ./Core/Network/net_socket.cyclo ./Core/Network/net_socket.d ./Core/Network/net_socket.o ./Core/Network/net_socket.su

.PHONY: clean-Core-2f-Network

