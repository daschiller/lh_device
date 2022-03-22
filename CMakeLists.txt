cmake_minimum_required(VERSION 3.20.0)

# create compile_commands.json for VS Code
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BOARD wuerth_proteus-iii)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lh_device)

target_sources(app PRIVATE
  src/main.c
  src/ble.c
  src/pwm.c
)
