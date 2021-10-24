cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(TestTBPipe VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "-g2")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")

get_filename_component(PIPE_ROOT
                       "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/TBPipe"
                       ABSOLUTE)

include_directories(${PIPE_ROOT})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../emulation)



set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/TestTBPipe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TestTBinParse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FillBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../emulation/sysctl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../emulation/sleep.cpp
    ${PIPE_ROOT}/TBPipe.cpp
    ${PIPE_ROOT}/TBParse.cpp
    ${PIPE_ROOT}/TBinParse.cpp
  )

add_executable(TestTBPipe ${SOURCE})