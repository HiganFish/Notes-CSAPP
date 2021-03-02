cmake_minimum_required(VERSION 3.17)
project(csapp)

set(CMAKE_CXX_STANDARD 20)

include_directories(format/include)

add_executable(TinyWeb TinyWeb.cpp Buffer.cpp)
target_link_libraries(TinyWeb format)

add_subdirectory(format)