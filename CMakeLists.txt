cmake_minimum_required(VERSION 3.16)
project(fire)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS, "${CMAKE_CXX_FLAGS} -O0 -g")

add_executable(fire main.cpp)

find_package(glfw3 3.3.2 REQUIRED)
target_link_libraries(fire glfw)
find_package(OpenGL REQUIRED)
# These variables have been set by the previous command
target_include_directories(fire PUBLIC ${OPENGL_INCLUDE_DIR})
target_link_libraries(fire ${OPENGL_LIBRARIES})
