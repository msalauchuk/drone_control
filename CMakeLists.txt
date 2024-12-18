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
# Add Redis dependencies
set(HIREDIS_ROOT "/opt/homebrew/opt/hiredis")
include_directories(${HIREDIS_ROOT}/include)
link_directories(${HIREDIS_ROOT}/lib)

# Common include directories
include_directories(
        ${CMAKE_SOURCE_DIR}src/redis/src
        ${CMAKE_SOURCE_DIR}src/control_center/src
        ${CMAKE_SOURCE_DIR}src/drone/src
        ${CMAKE_SOURCE_DIR}
)

# Common library sources
add_library(common_lib
        src/redis/src/readreply.cpp
        src/redis/src/redisfun.cpp
        src/redis/src/local.h
        src/redis/src/con2redis.h

)

# Control Center executable
add_executable(control_center
        src/control_center/src/cc.cpp
        src/control_center/src/main.cpp
        src/control_center/src/config.h
        src/control_center/src/cc.h

)

# Drone executable
add_executable(drone
        src/drone/src/Drone.cpp
        src/drone/src/main.cpp
        src/drone/src/config.h
        src/drone/src/Drone.h
)

# Link common library and hiredis to both executables
target_link_libraries(common_lib PRIVATE hiredis)
target_link_libraries(control_center PRIVATE common_lib hiredis)
target_link_libraries(drone PRIVATE common_lib hiredis)