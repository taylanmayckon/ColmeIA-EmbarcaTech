add_library(SGP40_Driver INTERFACE)

target_sources(SGP40_Driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/sgp40_i2c.c
    ${CMAKE_CURRENT_LIST_DIR}/sensirion_i2c.c
    ${CMAKE_CURRENT_LIST_DIR}/sensirion_common.c
    ${CMAKE_CURRENT_LIST_DIR}/sensirion_i2c_hal.c 
    ${CMAKE_CURRENT_LIST_DIR}/sensirion_gas_index_algorithm.c
)

target_include_directories(SGP40_Driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR} 
)

target_link_libraries(SGP40_Driver INTERFACE
    pico_stdlib
    hardware_i2c 
)