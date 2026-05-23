cmake_minimum_required(VERSION 3.18)
project(ecpri_dpdk_spectrum CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

include_directories(include)

add_subdirectory(src/generator)
add_subdirectory(src/receiver)
