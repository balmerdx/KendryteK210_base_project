cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(bterminal VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../drivers/TBPipe)
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../emulation)

set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/serial.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/network_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../drivers/TBPipe/TBParse.cpp
  )

add_executable(bterminal ${SOURCE})
target_link_libraries(bterminal libpng.so)
