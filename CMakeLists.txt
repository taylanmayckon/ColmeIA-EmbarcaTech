# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
project(ApiSSense C CXX ASM)
pico_sdk_init()

# Adição do FreeRTOS no CMakeLists
set(FREERTOS_KERNEL_PATH "${CMAKE_SOURCE_DIR}/lib/FreeRTOS-Kernel-11.2.0")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

include_directories( ${CMAKE_SOURCE_DIR}/lib ) 

add_executable(ApiSSense ApiSSense.cpp lib/MCP23017.cpp lib/HX711.cpp)

pico_generate_pio_header(ApiSSense ${CMAKE_CURRENT_LIST_DIR}/lib/hx711.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

pico_set_program_name(ApiSSense "ApiSSense")
pico_set_program_version(ApiSSense "0.1")

pico_enable_stdio_uart(ApiSSense 1)
pico_enable_stdio_usb(ApiSSense 1)

target_link_libraries(ApiSSense
        pico_stdlib
        hardware_gpio
        hardware_i2c
        hardware_pio
        hardware_clocks
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4)

target_include_directories(ApiSSense PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(ApiSSense)