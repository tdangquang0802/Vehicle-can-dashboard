################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/buttons.c \
../Core/Src/can_drv.c \
../Core/Src/common.c \
../Core/Src/diag_app.c \
../Core/Src/diag_io.c \
../Core/Src/dtc_text.c \
../Core/Src/main.c \
../Core/Src/obd_client.c \
../Core/Src/stm32f1xx_hal_msp.c \
../Core/Src/stm32f1xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f1xx.c \
../Core/Src/tft.c \
../Core/Src/tft_font.c \
../Core/Src/tick_hal.c \
../Core/Src/ui.c \
../Core/Src/wdg.c 

OBJS += \
./Core/Src/buttons.o \
./Core/Src/can_drv.o \
./Core/Src/common.o \
./Core/Src/diag_app.o \
./Core/Src/diag_io.o \
./Core/Src/dtc_text.o \
./Core/Src/main.o \
./Core/Src/obd_client.o \
./Core/Src/stm32f1xx_hal_msp.o \
./Core/Src/stm32f1xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f1xx.o \
./Core/Src/tft.o \
./Core/Src/tft_font.o \
./Core/Src/tick_hal.o \
./Core/Src/ui.o \
./Core/Src/wdg.o 

C_DEPS += \
./Core/Src/buttons.d \
./Core/Src/can_drv.d \
./Core/Src/common.d \
./Core/Src/diag_app.d \
./Core/Src/diag_io.d \
./Core/Src/dtc_text.d \
./Core/Src/main.d \
./Core/Src/obd_client.d \
./Core/Src/stm32f1xx_hal_msp.d \
./Core/Src/stm32f1xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f1xx.d \
./Core/Src/tft.d \
./Core/Src/tft_font.d \
./Core/Src/tick_hal.d \
./Core/Src/ui.d \
./Core/Src/wdg.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/buttons.cyclo ./Core/Src/buttons.d ./Core/Src/buttons.o ./Core/Src/buttons.su ./Core/Src/can_drv.cyclo ./Core/Src/can_drv.d ./Core/Src/can_drv.o ./Core/Src/can_drv.su ./Core/Src/common.cyclo ./Core/Src/common.d ./Core/Src/common.o ./Core/Src/common.su ./Core/Src/diag_app.cyclo ./Core/Src/diag_app.d ./Core/Src/diag_app.o ./Core/Src/diag_app.su ./Core/Src/diag_io.cyclo ./Core/Src/diag_io.d ./Core/Src/diag_io.o ./Core/Src/diag_io.su ./Core/Src/dtc_text.cyclo ./Core/Src/dtc_text.d ./Core/Src/dtc_text.o ./Core/Src/dtc_text.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/obd_client.cyclo ./Core/Src/obd_client.d ./Core/Src/obd_client.o ./Core/Src/obd_client.su ./Core/Src/stm32f1xx_hal_msp.cyclo ./Core/Src/stm32f1xx_hal_msp.d ./Core/Src/stm32f1xx_hal_msp.o ./Core/Src/stm32f1xx_hal_msp.su ./Core/Src/stm32f1xx_it.cyclo ./Core/Src/stm32f1xx_it.d ./Core/Src/stm32f1xx_it.o ./Core/Src/stm32f1xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f1xx.cyclo ./Core/Src/system_stm32f1xx.d ./Core/Src/system_stm32f1xx.o ./Core/Src/system_stm32f1xx.su ./Core/Src/tft.cyclo ./Core/Src/tft.d ./Core/Src/tft.o ./Core/Src/tft.su ./Core/Src/tft_font.cyclo ./Core/Src/tft_font.d ./Core/Src/tft_font.o ./Core/Src/tft_font.su ./Core/Src/tick_hal.cyclo ./Core/Src/tick_hal.d ./Core/Src/tick_hal.o ./Core/Src/tick_hal.su ./Core/Src/ui.cyclo ./Core/Src/ui.d ./Core/Src/ui.o ./Core/Src/ui.su ./Core/Src/wdg.cyclo ./Core/Src/wdg.d ./Core/Src/wdg.o ./Core/Src/wdg.su

.PHONY: clean-Core-2f-Src

