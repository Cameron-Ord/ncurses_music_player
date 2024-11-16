cmake_minimum_required(VERSION 3.16)
project(jinx VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(CMAKE_REQUIRED_FLAGS "-lncurses -lm")
file(GLOB SOURCES "src/*.c")

add_executable(jinx ${SOURCES})
target_compile_options(jinx PRIVATE -Wall -Wextra -pedantic)

target_link_libraries(jinx m)
find_library(NCURSES_LIB NAMES ncurses libncurses)
find_path(NCURSES_INC NAMES ncurses ncurses.h)

message("")

if(NOT NCURSES_INC)
  message(FATAL_ERROR "ncurses includes not found!")
else()
  message(STATUS "ncurses includes found at ${NCURSES_INC}")
  include_directories(${NCURSES_INC})
endif()

if(NOT NCURSES_LIB)
  message(FATAL_ERROR "ncurses library not found!")
else()
  message(STATUS "ncurses library found at ${NCURSES_LIB}")
  target_link_libraries(jinx ${NCURSES_LIB})
endif()

message("")
