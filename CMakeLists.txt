cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Simple_wireless_automatic_transceiver_deviceV2.0)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)



# LVGL root
set(LVGL_DIR ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/lvgl_9.5.0)

# Collect LVGL core sources
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
    "${LVGL_DIR}/src/*.c"
)

# Create LVGL static library
add_library(lvgl STATIC ${LVGL_SOURCES})

target_include_directories(lvgl PUBLIC
    "${LVGL_DIR}"
    "${CMAKE_SOURCE_DIR}/Core/Inc"
    "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include"
    "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/RTOS2/Include"
)


target_compile_definitions(lvgl PUBLIC
    LV_CONF_INCLUDE_SIMPLE
    LV_KCONFIG_IGNORE
)



# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
     Core/App/LTDC/app_display.c
     Core/App/MODULATE/Modulate.c
     Core/App/SDRAM/app_sdram_test.c
     Core/App/WinnerBridge/app_winner_bridge.c
     Core/App/Tasks/app_adc_task.c
     Core/App/Tasks/app_ad9959_task.c
     Core/App/Tasks/app_display_task.c
     Core/App/Tasks/app_Modulate_Task.c
     Core/App/Touch/app_touch_gt9xx.c
     Core/App/LVGL/app_lvgl.c
     Core/App/LVGL/app_lvgl_port_disp.c
     Core/App/LVGL/app_lvgl_port_indev.c
     Core/App/LVGL/app_lvgl_ui.c
     Core/App/Attenuator/app_pe4302.c
     Core/App/BoardFlash/app_board_flash.c
     Core/App/BoardFlash/board_flash_qspi_port.c
     Core/App/BoardFlash/board_flash_w25q64.c
     Core/App/TxControl/app_cw_amp_cal.c
     Core/App/TxControl/app_tx_control.c
     Core/App/Uart/app_uart_cli.c
     Core/App/DAC/app_dac_wavegen.c
     Core/App/DDS_9959/App_9959/app_ad9959.c
     Core/App/DDS_9959/App_9959/app_dds_ctrl.c
     Core/App/DDS_9959/Hardware/ad9959.c
     Core/App/DDS_9959/Hardware/ad9959_port.c
     Core/App/SI5351/Legacy/common/si5351_legacy_common.c
     Core/App/SI5351/Legacy/basic/SI5351.c
     Core/App/SI5351/Legacy/pro/SI5351PRO.c
     Core/App/SI5351/Legacy/promax/SI5351PROMAX.c
     Core/App/SI5351/Port/si5351_port.c
     Core/App/SI5351/Wrapper/app_si5351_drv.c
 )

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
     Core/App/LTDC
     Core/App/MODULATE
     Core/App/SDRAM
     Core/App/WinnerBridge
     Core/App/Tasks
     Core/App/Touch
     Core/App/LVGL
     Core/App/Attenuator
     Core/App/BoardFlash
     Core/App/TxControl
     Core/App/Uart
     Core/App/DAC
     Core/App/DDS_9959/App_9959
     Core/App/DDS_9959/Hardware
     Core/App/SI5351/Config
     Core/App/SI5351/Legacy/basic
     Core/App/SI5351/Legacy/common
     Core/App/SI5351/Legacy/pro
     Core/App/SI5351/Legacy/promax
     Core/App/SI5351/Port
     Core/App/SI5351/Wrapper
     Core/Inc
 
     Core/App/Tasks/Types
 )

set_source_files_properties(Core/App/DAC/app_dac_wavegen.c PROPERTIES
    COMPILE_DEFINITIONS "StartAdcTask=App_DacWavegenLegacyStartAdcTask"
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl
    # Add user defined libraries
)
