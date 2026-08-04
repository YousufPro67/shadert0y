cmake_minimum_required(VERSION 3.10)
project(shadert0y CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)

add_library(shadert0y MODULE shadert0y.cpp)
target_include_directories(shadert0y PRIVATE ${EGL_INCLUDE_DIRS})
target_link_libraries(shadert0y PRIVATE ${EGL_LIBRARIES} GL dl)
set_target_properties(shadert0y PROPERTIES PREFIX "")

install(TARGETS shadert0y LIBRARY DESTINATION lib/frei0r-1)

message(STATUS "")
message(STATUS "Built target: shadert0y.so")
message(STATUS "")
