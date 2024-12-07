cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

set(FREERTOS_SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/FreeRTOS-Kernel")

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Add FreeRTOS as a library
add_library(FreeRTOS STATIC
    ${FREERTOS_SRC_DIRECTORY}/event_groups.c
    ${FREERTOS_SRC_DIRECTORY}/list.c
    ${FREERTOS_SRC_DIRECTORY}/queue.c
    ${FREERTOS_SRC_DIRECTORY}/stream_buffer.c
    ${FREERTOS_SRC_DIRECTORY}/tasks.c
    ${FREERTOS_SRC_DIRECTORY}/timers.c
    ${FREERTOS_SRC_DIRECTORY}/portable/MemMang/heap_4.c
    ${FREERTOS_SRC_DIRECTORY}/portable/GCC/ARM_CM0/port.c
)

# Build FreeRTOS
target_include_directories(FreeRTOS PUBLIC
	"src"
    ${FREERTOS_SRC_DIRECTORY}/include
    ${FREERTOS_SRC_DIRECTORY}/portable/GCC/ARM_CM0
)

add_definitions(
-DFW_MAJOR=0
-DFW_MINOR=2
-DFW_PATCH=2
)

add_executable(LibreCAL 
	src/main.cpp
	src/Switch.cpp
	src/Heater.cpp
	src/SCPI.cpp
	src/serial.c
	src/freertos.c
	src/Touchstone.cpp
	src/Flash.cpp
	src/UserInterface.cpp
	src/USB/msc_disk.cpp
	src/USB/usb_descriptors.c
	src/USB/usb.c
	src/fatfs/ff.c
	src/fatfs/ffsystem.c
	src/fatfs/ffunicode.c
	src/fatfs/flashdisk.cpp
)

target_include_directories(LibreCAL PUBLIC
	src
	src/USB
	src/fatfs
    ${FREERTOS_SRC_DIRECTORY}/include
)

pico_add_extra_outputs(LibreCAL)
target_link_libraries(LibreCAL pico_stdlib pico_unique_id hardware_rtc hardware_spi hardware_pwm hardware_adc FreeRTOS tinyusb_device tinyusb_board)
