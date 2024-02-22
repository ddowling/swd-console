cmake_minimum_required(VERSION 3.21)

# Setup paths to arduino environment and libraries
file(REAL_PATH "~/.arduino15/packages/STMicroelectronics/hardware/stm32/2.7.1" CORE_PATH EXPAND_TILDE)
file(TO_CMAKE_PATH "${CORE_PATH}" CORE_PATH)

file(REAL_PATH "~/Arduino/libraries" USER_LIBS EXPAND_TILDE)
file(TO_CMAKE_PATH "${USER_LIBS}" USER_LIBS)

# Setup the cmake module path and toolchain
# FIXME Testing cmake fixes
#list(APPEND CMAKE_MODULE_PATH ${CORE_PATH}/cmake)
list(APPEND CMAKE_MODULE_PATH /home/dpd/opsol_projects/Arduino_Core_STM32/cmake)
set(CMAKE_TOOLCHAIN_FILE toolchain)

# You may remove this block when using this file as the sketch's CMakeLists.txt
if (NOT ${CMAKE_PARENT_LIST_FILE} STREQUAL ${CMAKE_CURRENT_LIST_FILE})
    # When we are imported from the main CMakeLists.txt, we should stop here
    # not to interfere with the true build config.
    return()
endif()

project("swd-console")

include(set_board)

# Options
# SERIAL generic / disabled / none
# USB none / CDCgen / CDC / HID
# XUSB FS / HS / HSFS
# VIRTIO disable / generic / enabled
# BOOTLOADER dfuo / dfu2 / hid
set_board("BLUEPILL_F103CB"
  USB none
)

include(overall_settings)
overall_settings(
  # STANDARD_LIBC
  # PRINTF_FLOAT
  # SCANF_FLOAT
  # DEBUG_SYMBOLS
  # LTO
  # NO_RELATIVE_MACRO
  # UNDEF_NDEBUG
  # OPTIMIZATION "s"
  # BUILD_OPT ./build.opt
  # DISABLE_HAL_MODULES ADC I2C RTC SPI TIM DAC EXTI ETH SD QSPI
  # CORE_CALLBACK
)

include_directories(src)

include(build_sketch)
build_sketch(TARGET flash_led
  SOURCES
  examples/flash_led/flash_led.ino
  examples/flash_led/flash_led.cpp
  src/SWDStream.cpp src/SWDStream.h
)

build_sketch(TARGET test_command
  SOURCES
  examples/test_command/test_command.ino
  examples/test_command/test_command.cpp
  src/SWDStream.cpp src/SWDStream.h
  src/CommandParser.cpp src/CommandParser.h
)
