# CNC контролер — firmware за STM32H723ZG (изпълнителната част)
# Copyright (c) 2026 ageorgiev86. Всички права запазени.

cmake_minimum_required(VERSION 3.22)

project(cnc_mcu C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(DRV ${CMAKE_SOURCE_DIR}/Drivers)
set(HAL ${DRV}/STM32H7xx_HAL_Driver)
set(DEV ${DRV}/CMSIS/Device/ST/STM32H7xx)

if(NOT EXISTS ${HAL}/Src/stm32h7xx_hal.c)
  message(FATAL_ERROR "Липсват библиотеките на ST. Изпълнете: git submodule update --init --recursive")
endif()

# Cortex-M7 с FPU с двойна точност
set(MCU_FLAGS -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb)

set(HAL_SOURCES
  ${HAL}/Src/stm32h7xx_hal.c
  ${HAL}/Src/stm32h7xx_hal_cortex.c
  ${HAL}/Src/stm32h7xx_hal_rcc.c
  ${HAL}/Src/stm32h7xx_hal_rcc_ex.c
  ${HAL}/Src/stm32h7xx_hal_pwr.c
  ${HAL}/Src/stm32h7xx_hal_pwr_ex.c
  ${HAL}/Src/stm32h7xx_hal_gpio.c
  ${HAL}/Src/stm32h7xx_hal_flash.c
  ${HAL}/Src/stm32h7xx_hal_flash_ex.c
  ${HAL}/Src/stm32h7xx_hal_dma.c
  ${HAL}/Src/stm32h7xx_hal_dma_ex.c
  ${HAL}/Src/stm32h7xx_hal_mdma.c
  ${HAL}/Src/stm32h7xx_hal_exti.c
  ${HAL}/Src/stm32h7xx_hal_hsem.c
  ${HAL}/Src/stm32h7xx_hal_tim.c
  ${HAL}/Src/stm32h7xx_hal_tim_ex.c
  ${HAL}/Src/stm32h7xx_hal_spi.c
  ${HAL}/Src/stm32h7xx_hal_spi_ex.c
  ${HAL}/Src/stm32h7xx_hal_uart.c
  ${HAL}/Src/stm32h7xx_hal_uart_ex.c
  ${HAL}/Src/stm32h7xx_hal_iwdg.c
  ${HAL}/Src/stm32h7xx_hal_pcd.c
  ${HAL}/Src/stm32h7xx_hal_pcd_ex.c
  ${HAL}/Src/stm32h7xx_ll_usb.c
  ${HAL}/Src/stm32h7xx_hal_adc.c
  ${HAL}/Src/stm32h7xx_hal_adc_ex.c
)

# собственият код (компилира се с повече предупреждения)
set(OWN_SOURCES
  Core/Src/main.c
  Core/Src/stm32h7xx_it.c
  Core/Src/stm32h7xx_hal_msp.c
  Core/Src/syscalls.c
  Core/Src/usb_cdc.c
  Core/Src/console.c
  Core/Src/stepgen.c
  Core/Src/resume.c
  Core/Src/inputs.c
  Core/Src/link.c
  Core/Src/homing.c
  Core/Src/machio.c
)

set(APP_SOURCES
  ${OWN_SOURCES}
  ${DEV}/Source/Templates/system_stm32h7xx.c
  ${DEV}/Source/Templates/gcc/startup_stm32h723xx.s
)

add_executable(${PROJECT_NAME} ${APP_SOURCES} ${HAL_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_compile_definitions(${PROJECT_NAME} PRIVATE USE_HAL_DRIVER STM32H723xx
  $<$<CONFIG:Debug>:DEBUG>)

# платката: основната (по подразбиране) или тестовата 3-осна машина (конфигурация Test3)
set(CNC_BOARD "osnovna" CACHE STRING "платката: osnovna или test3")
if(CNC_BOARD STREQUAL "test3")
  target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_TEST3)
elseif(NOT CNC_BOARD STREQUAL "osnovna")
  message(FATAL_ERROR "Непозната платка CNC_BOARD=${CNC_BOARD} (osnovna или test3)")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
  Core/Inc
  ${HAL}/Inc
  ${DEV}/Include
  ${DRV}/CMSIS/Core_repo/Core/Include
)

target_compile_options(${PROJECT_NAME} PRIVATE ${MCU_FLAGS}
  -Wall -fdata-sections -ffunction-sections
  $<$<CONFIG:Debug>:-Og -g3>
  $<$<CONFIG:Release>:-O2 -g>
)
# собственият код се компилира с повече предупреждения
set_source_files_properties(${OWN_SOURCES}
  PROPERTIES COMPILE_OPTIONS "-Wextra;-Wshadow;-Wpointer-arith;-Wstrict-prototypes;-Wmissing-prototypes;-Wundef")

set(LD_SCRIPT ${CMAKE_SOURCE_DIR}/STM32H723ZGTX_FLASH.ld)
target_link_options(${PROJECT_NAME} PRIVATE ${MCU_FLAGS}
  -T${LD_SCRIPT}
  --specs=nano.specs --specs=nosys.specs
  -Wl,--gc-sections
  -Wl,-Map=${PROJECT_NAME}.map,--cref
  -Wl,--print-memory-usage
  -Wl,--no-warn-rwx-segments
)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${LD_SCRIPT})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
  COMMENT "Създаване на .hex и .bin")
