################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Network/Services/dhcp_service.c \
../Core/Network/Services/dns_service.c \
../Core/Network/Services/ntp_service.c \
../Core/Network/Services/telnet_service.c 

OBJS += \
./Core/Network/Services/dhcp_service.o \
./Core/Network/Services/dns_service.o \
./Core/Network/Services/ntp_service.o \
./Core/Network/Services/telnet_service.o 

C_DEPS += \
./Core/Network/Services/dhcp_service.d \
./Core/Network/Services/dns_service.d \
./Core/Network/Services/ntp_service.d \
./Core/Network/Services/telnet_service.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Network/Services/%.o Core/Network/Services/%.su Core/Network/Services/%.cyclo: ../Core/Network/Services/%.c Core/Network/Services/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG '-DCMSIS_device_header=<stm32g0xx.h>' -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../Core/Network -I../Core/Network/Driver/W5500 -I../Core/Network/Services -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Network-2f-Services

clean-Core-2f-Network-2f-Services:
	-$(RM) ./Core/Network/Services/dhcp_service.cyclo ./Core/Network/Services/dhcp_service.d ./Core/Network/Services/dhcp_service.o ./Core/Network/Services/dhcp_service.su ./Core/Network/Services/dns_service.cyclo ./Core/Network/Services/dns_service.d ./Core/Network/Services/dns_service.o ./Core/Network/Services/dns_service.su ./Core/Network/Services/ntp_service.cyclo ./Core/Network/Services/ntp_service.d ./Core/Network/Services/ntp_service.o ./Core/Network/Services/ntp_service.su ./Core/Network/Services/telnet_service.cyclo ./Core/Network/Services/telnet_service.d ./Core/Network/Services/telnet_service.o ./Core/Network/Services/telnet_service.su

.PHONY: clean-Core-2f-Network-2f-Services

