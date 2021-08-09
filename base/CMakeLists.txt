# set this will supress some warnings
set(BUILDING_SDK "yes" CACHE INTERNAL "")
get_filename_component(SDK_ROOT
                       "${CMAKE_CURRENT_LIST_DIR}/../sdk"
                       ABSOLUTE)
# message(${SDK_ROOT})
get_filename_component(LD_LINK_FILE
                       "${CMAKE_CURRENT_LIST_DIR}/lds/kendryte.ld"
                       ABSOLUTE)

# basic config
if (NOT PROJ)
    get_filename_component(PROJ ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)
    get_filename_component(PROJ ${PROJ} NAME)
    string(REPLACE " " "_" PROJ ${PROJ})
    message(STATUS "PROJ not set, use ${PROJ} as PROJ. Also, you can set it manually. e.g. -DPROJ=hello_world")
else()
    message("PROJ = ${PROJ}")
endif ()
cmake_minimum_required(VERSION 3.0)
include(${SDK_ROOT}/cmake/common.cmake)
project(${PROJ} C CXX ASM)

add_compile_flags(C -std=gnu11 -save-temps=obj)
# config self use headers
include(${SDK_ROOT}/cmake/macros.internal.cmake)
header_directories(${SDK_ROOT}/lib)
header_directories(src/${PROJ})
header_directories(kendryte-standalone-demo/${PROJ})
# build library first
add_subdirectory(${SDK_ROOT}/lib lib)

# compile project
message("LD_LINK_FILE=" ${LD_LINK_FILE})

add_source_files(src/*.c src/*.s src/*.S src/*.cpp)
include(${SDK_ROOT}/cmake/executable.cmake)
