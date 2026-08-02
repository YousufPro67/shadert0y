cmake_minimum_required(VERSION 3.10)
project(shadert0y CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)

add_library(shadertoy MODULE shadert0y.cpp)
target_include_directories(shadertoy PRIVATE ${EGL_INCLUDE_DIRS})
target_link_libraries(shadertoy PRIVATE ${EGL_LIBRARIES} GL dl)
set_target_properties(shadertoy PROPERTIES PREFIX "")

install(TARGETS shadertoy LIBRARY DESTINATION lib/frei0r-1)

message(STATUS "")
message(STATUS "Built target: shadertoy.so")
message(STATUS "Install to a frei0r plugin dir, e.g.:")
message(STATUS "  mkdir -p ~/.frei0r-1/lib && cp shadertoy.so ~/.frei0r-1/lib/")
message(STATUS "")
