cmake_minimum_required(VERSION 3.10.0)
project(walkdir VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(walkdir
    ${CMAKE_SOURCE_DIR}/src/main.cpp
)

target_include_directories(walkdir PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# target_link_libraries(walkdir
#     pthread
# )