################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/can_drv.c \
../Core/Src/common.c \
../Core/Src/ecu_app.c \
../Core/Src/fault_mgr.c \
../Core/Src/main.c \
../Core/Src/obd_srv.c \
../Core/Src/sim.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/tick_hal.c \
../Core/Src/wdg.c 

OBJS += \
./Core/Src/can_drv.o \
./Core/Src/common.o \
./Core/Src/ecu_app.o \
./Core/Src/fault_mgr.o \
./Core/Src/main.o \
./Core/Src/obd_srv.o \
./Core/Src/sim.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/tick_hal.o \
./Core/Src/wdg.o 

C_DEPS += \
./Core/Src/can_drv.d \
./Core/Src/common.d \
./Core/Src/ecu_app.d \
./Core/Src/fault_mgr.d \
./Core/Src/main.d \
./Core/Src/obd_srv.d \
./Core/Src/sim.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/tick_hal.d \
./Core/Src/wdg.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/can_drv.cyclo ./Core/Src/can_drv.d ./Core/Src/can_drv.o ./Core/Src/can_drv.su ./Core/Src/common.cyclo ./Core/Src/common.d ./Core/Src/common.o ./Core/Src/common.su ./Core/Src/ecu_app.cyclo ./Core/Src/ecu_app.d ./Core/Src/ecu_app.o ./Core/Src/ecu_app.su ./Core/Src/fault_mgr.cyclo ./Core/Src/fault_mgr.d ./Core/Src/fault_mgr.o ./Core/Src/fault_mgr.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/obd_srv.cyclo ./Core/Src/obd_srv.d ./Core/Src/obd_srv.o ./Core/Src/obd_srv.su ./Core/Src/sim.cyclo ./Core/Src/sim.d ./Core/Src/sim.o ./Core/Src/sim.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/tick_hal.cyclo ./Core/Src/tick_hal.d ./Core/Src/tick_hal.o ./Core/Src/tick_hal.su ./Core/Src/wdg.cyclo ./Core/Src/wdg.d ./Core/Src/wdg.o ./Core/Src/wdg.su

.PHONY: clean-Core-2f-Src

