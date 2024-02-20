
find_package(PkgConfig REQUIRED)

set(CMAKE_CHIPS_DIR /usr/local/share/stlink/chips)
add_definitions(-DSTLINK_CHIPS_DIR="${CMAKE_CHIPS_DIR}")

pkg_check_modules(LIBUSB "libusb-1.0" REQUIRED)

include_directories(
  "/usr/local/include/stlink"
  ${LIBUSB_INCLUDE_DIRS})

add_executable(monitor
  monitor.cpp
  STLink.cpp)

target_link_libraries(monitor
  /usr/local/lib/libstlink.a
  ${LIBUSB_LIBRARIES})


