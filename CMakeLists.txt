cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/px1122r
  )

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ZDebug)
endif()

set(CONF_FILE "prj.${CMAKE_BUILD_TYPE}.conf" "prj.conf")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(diy_gnss_v2)

target_sources(app PRIVATE src/main.c src/events/gnss_event.c src/modules/gnss_module.c)

zephyr_include_directories(
  configuration/${BOARD}  # Add include directory for board specific CAF def files
  include/events
  )
