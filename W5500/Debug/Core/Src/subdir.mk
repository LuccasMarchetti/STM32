################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/app_freertos.c \
../Core/Src/dma.c \
../Core/Src/ethernet_dhcp.c \
../Core/Src/ethernet_dns.c \
../Core/Src/ethernet_mdns.c \
../Core/Src/ethernet_ntp.c \
../Core/Src/gpio.c \
../Core/Src/main.c \
../Core/Src/network_manager.c \
../Core/Src/spi.c \
../Core/Src/stm32g0xx_hal_msp.c \
../Core/Src/stm32g0xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32g0xx.c \
../Core/Src/w5500.c 

OBJS += \
./Core/Src/app_freertos.o \
./Core/Src/dma.o \
./Core/Src/ethernet_dhcp.o \
./Core/Src/ethernet_dns.o \
./Core/Src/ethernet_mdns.o \
./Core/Src/ethernet_ntp.o \
./Core/Src/gpio.o \
./Core/Src/main.o \
./Core/Src/network_manager.o \
./Core/Src/spi.o \
./Core/Src/stm32g0xx_hal_msp.o \
./Core/Src/stm32g0xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32g0xx.o \
./Core/Src/w5500.o 

C_DEPS += \
./Core/Src/app_freertos.d \
./Core/Src/dma.d \
./Core/Src/ethernet_dhcp.d \
./Core/Src/ethernet_dns.d \
./Core/Src/ethernet_mdns.d \
./Core/Src/ethernet_ntp.d \
./Core/Src/gpio.d \
./Core/Src/main.d \
./Core/Src/network_manager.d \
./Core/Src/spi.d \
./Core/Src/stm32g0xx_hal_msp.d \
./Core/Src/stm32g0xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32g0xx.d \
./Core/Src/w5500.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG '-DCMSIS_device_header=<stm32g0xx.h>' -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app_freertos.cyclo ./Core/Src/app_freertos.d ./Core/Src/app_freertos.o ./Core/Src/app_freertos.su ./Core/Src/dma.cyclo ./Core/Src/dma.d ./Core/Src/dma.o ./Core/Src/dma.su ./Core/Src/ethernet_dhcp.cyclo ./Core/Src/ethernet_dhcp.d ./Core/Src/ethernet_dhcp.o ./Core/Src/ethernet_dhcp.su ./Core/Src/ethernet_dns.cyclo ./Core/Src/ethernet_dns.d ./Core/Src/ethernet_dns.o ./Core/Src/ethernet_dns.su ./Core/Src/ethernet_mdns.cyclo ./Core/Src/ethernet_mdns.d ./Core/Src/ethernet_mdns.o ./Core/Src/ethernet_mdns.su ./Core/Src/ethernet_ntp.cyclo ./Core/Src/ethernet_ntp.d ./Core/Src/ethernet_ntp.o ./Core/Src/ethernet_ntp.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/network_manager.cyclo ./Core/Src/network_manager.d ./Core/Src/network_manager.o ./Core/Src/network_manager.su ./Core/Src/spi.cyclo ./Core/Src/spi.d ./Core/Src/spi.o ./Core/Src/spi.su ./Core/Src/stm32g0xx_hal_msp.cyclo ./Core/Src/stm32g0xx_hal_msp.d ./Core/Src/stm32g0xx_hal_msp.o ./Core/Src/stm32g0xx_hal_msp.su ./Core/Src/stm32g0xx_it.cyclo ./Core/Src/stm32g0xx_it.d ./Core/Src/stm32g0xx_it.o ./Core/Src/stm32g0xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32g0xx.cyclo ./Core/Src/system_stm32g0xx.d ./Core/Src/system_stm32g0xx.o ./Core/Src/system_stm32g0xx.su ./Core/Src/w5500.cyclo ./Core/Src/w5500.d ./Core/Src/w5500.o ./Core/Src/w5500.su

.PHONY: clean-Core-2f-Src

