cmake_minimum_required(VERSION 3.9)
project(2PL)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


set(2PL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(2PL_LIB 2pl_lib)
add_library(${2PL_LIB} INTERFACE)
target_include_directories(${2PL_LIB} INTERFACE
  ${2PL_INCLUDE_DIR}
)


set(2PL_EXEC 2pl_test)
add_executable(${2PL_EXEC} 
    ${CMAKE_CURRENT_SOURCE_DIR}/test/test.cpp)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


