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

find_library(SNDFILE_LIB NAMES libsndfile sndfile)
find_path(SNDFILE_INC NAMES libsndfile.h sndfile.h)

find_library(PIPEWIRE_LIB NAMES libpipewire pipewire libpipewire-0.3)
find_path(PIPEWIRE_INC NAMES pipewire.h pipewire/pipewire.h)

find_library(NCURSES_LIB NAMES ncurses libncurses)
find_path(NCURSES_INC NAMES ncurses ncurses.h)

message("")

if(NOT PIPEWIRE_INC)
  message(FATAL_ERROR "pipewire includes not found!")
else()
  message(STATUS "pipewire includes found at ${PIPEWIRE_INC}")
  include_directories(${PIPEWIRE_INC})
endif()

if(NOT PIPEWIRE_LIB)
  message(FATAL_ERROR "pipewire library not found!")
else()
  message(STATUS "pipewire library found at ${PIPEWIRE_LIB}")
  target_link_libraries(jinx ${PIPEWIRE_LIB})
endif()


if(NOT SNDFILE_INC)
  message(FATAL_ERROR "sndfile includes not found!")
else()
  message(STATUS "sndfile includes found at ${SNDFILE_INC}")
  include_directories(${SNDFILE_INC})
endif()

if(NOT SNDFILE_LIB)
  message(FATAL_ERROR "sndfile library not found!")
else()
  message(STATUS "sndfile library found at ${SNDFILE_LIB}")
  target_link_libraries(jinx ${SNDFILE_LIB})
endif()


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
