cmake_minimum_required(VERSION 3.16)
project(smart_device_simulator)

# Simulator uses LVGL SDL for desktop testing of GUI without hardware
# Requires SDL2 installed

find_package(SDL2 QUIET)

add_executable(simulator main.cpp)

# Mock HAL - no ESP-IDF deps
target_include_directories(simulator PRIVATE
  ../components/common/include
  ../components/gui/include
)

# For real simulator, add lvgl and sdl driver
# This is placeholder for GUI team: test clock, animations, command popups
