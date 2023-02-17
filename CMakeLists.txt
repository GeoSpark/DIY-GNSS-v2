cmake_minimum_required(VERSION 3.20.0)

# set(ZEPHYR_EXTRA_MODULES drivers/px1122r)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(diy_gnss_v2)

add_subdirectory(drivers)

zephyr_include_directories(drivers)

target_sources(app PRIVATE src/main.c)
