cmake_minimum_required(VERSION 3.29)

# Imposta i compilatori Clang
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

project(controllo_droni_master)

# Impostazioni del compilatore
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

# Impostazioni macOS
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")


# Redis configuration
set(HIREDIS_ROOT "/opt/homebrew/opt/hiredis")
find_package(hiredis REQUIRED)

# Common include directories
include_directories(
        ${CMAKE_SOURCE_DIR}/src/common/include
        ${CMAKE_SOURCE_DIR}/src/redis_connector/include
        ${CMAKE_SOURCE_DIR}/src/command_center/include
        ${CMAKE_SOURCE_DIR}/src/drone_controller/include
        ${CMAKE_SOURCE_DIR}
)

# Common library sources
add_library(common_lib
        src/redis_connector/src/readreply.cpp
        src/redis_connector/src/redis_client.cpp
        src/redis_connector/include/local.h
        src/redis_connector/include/connector.h

)


# Control Center executable
add_executable(control_center
        src/command_center/src/cc.cpp
        src/command_center/src/main.cpp
        src/command_center/include/config.h
        src/command_center/include/cc.h

)

# Drone executable
add_executable(drone
        src/drone_controller/src/drone.cpp
        src/drone_controller/src/main.cpp
        src/drone_controller/include/config.h
        src/drone_controller/include/drone.h
)

# Link common library and hiredis to both executables
target_link_libraries(common_lib PRIVATE hiredis)
target_link_libraries(control_center PRIVATE common_lib hiredis)
target_link_libraries(drone PRIVATE common_lib hiredis)