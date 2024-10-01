cmake_minimum_required(VERSION 3.21)
project(PS2RichPresence)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
add_executable(ps2rpc ${SOURCES})

target_include_directories(ps2rpc PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_directories(ps2rpc PRIVATE ${CMAKE_SOURCE_DIR}/lib)
target_compile_definitions(ps2rpc PRIVATE _XOPEN_SOURCE) # For strptime
target_compile_options(ps2rpc PRIVATE -pedantic-errors)
target_link_libraries(ps2rpc PRIVATE dl discord-rpc json-c)

# Add Apple ARM brew include/library paths
if (APPLE)
	target_include_directories(ps2rpc PRIVATE /opt/homebrew/include)
	target_link_directories(ps2rpc PRIVATE /opt/homebrew/lib)
endif (APPLE)
