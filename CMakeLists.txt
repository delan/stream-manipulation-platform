
cmake_minimum_required(VERSION 2.7)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories ("${PROJECT_SOURCE_DIR}/includes")

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

add_definitions( -D__DEBUG__ )

add_subdirectory("src")
add_executable(testing test.c)

target_link_libraries(testing sockets stringio eventmanager buffermanager pluginloader http heap class util)
