cmake_minimum_required(VERSION 3.29)
project(controllo_droni_master)

set(CMAKE_CXX_STANDARD 20)

# Add Redis dependencies
set(HIREDIS_ROOT "/opt/homebrew/opt/hiredis")
include_directories(${HIREDIS_ROOT}/include)
link_directories(${HIREDIS_ROOT}/lib)

# Common include directories
include_directories(
        ${CMAKE_SOURCE_DIR}/con2redis/src
        ${CMAKE_SOURCE_DIR}/control_center/src
        ${CMAKE_SOURCE_DIR}/drone/src
        ${CMAKE_SOURCE_DIR}
)

# Common library sources
add_library(common_lib
        con2redis/src/readreply.cpp
        con2redis/src/redisfun.cpp
)

# Control Center executable
add_executable(control_center
        control_center/src/ControlCenter.cpp
        control_center/src/main.cpp
)

# Drone executable
add_executable(drone
        drone/src/Drone.cpp
        drone/src/main.cpp
)

# Link common library and hiredis to both executables
target_link_libraries(common_lib PRIVATE hiredis)
target_link_libraries(control_center PRIVATE common_lib hiredis)
target_link_libraries(drone PRIVATE common_lib hiredis)