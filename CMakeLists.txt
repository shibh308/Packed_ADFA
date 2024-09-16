cmake_minimum_required(VERSION 3.22)
project(Packed_ADFA)

set(CMAKE_CXX_STANDARD 20)

set(SDSL_INCLUDE_DIR "./sdsl-lite/include")
set(SDSL_LIBRARY_DIR "./sdsl-lite/lib")

include_directories(${SDSL_INCLUDE_DIR})
link_directories(${SDSL_LIBRARY_DIR})

add_executable(Packed_ADFA main.cpp
        trie.hpp
        utils.hpp)

target_link_libraries(Packed_ADFA divsufsort divsufsort64 sdsl)
