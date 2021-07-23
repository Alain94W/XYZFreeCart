# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "../../pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(freecart C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(freecart freecart.c )

pico_set_program_name(freecart "freecart")
pico_set_program_version(freecart "0.1")

pico_enable_stdio_uart(freecart 1)
pico_enable_stdio_usb(freecart 1)

# Add the standard library to the build
target_link_libraries(freecart pico_stdlib)

#TM1637
include(./TM1637-pico/PicoTM1637.cmake)
#include(./unio_slave/UnioSlave.cmake)

# Add any user requested libraries
target_link_libraries(freecart
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_timer
        hardware_clocks
        hardware_flash 
        hardware_sync
        pico_multicore
        PicoTM1637
        #UnioSlave
        )

pico_add_extra_outputs(freecart)

if (NOT PICO_DEFAULT_BOOT_STAGE2_FILE)
    set(PICO_DEFAULT_BOOT_STAGE2_FILE "${PICO_SDK_PATH}/src/rp2_common/boot_stage2/boot2_w25q080.S")
endif()

if ( NOT EXISTS ${PICO_DEFAULT_BOOT_STAGE2_FILE})
    message(FATAL_ERROR "Specified boot_stage2 source '${PICO_BOOT_STAGE2_FILE}' does not exist.")
endif()
pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)

pico_set_boot_stage2(freecart slower_boot2)


pico_set_binary_type(freecart copy_to_ram) 
