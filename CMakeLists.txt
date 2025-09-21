cmake_minimum_required(VERSION 3.16)
project(udp-latency-benchmark VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(include)

# Find required packages
find_package(Threads REQUIRED)

# Library sources
set(LIBRARY_SOURCES
    src/core/common.cpp
    src/network/network_utils.cpp
    src/network/packet.cpp
    src/reliability/congestion_control.cpp
    src/reliability/reliability.cpp
    src/utils/stats.cpp
)

# Create the shared library
add_library(udp_benchmark_lib ${LIBRARY_SOURCES})
target_link_libraries(udp_benchmark_lib Threads::Threads)

# Executables
add_executable(udp_sender_new src/udp_sender_main.cpp)
target_link_libraries(udp_sender_new udp_benchmark_lib Threads::Threads)

add_executable(udp_receiver_new src/udp_receiver_main.cpp)
target_link_libraries(udp_receiver_new udp_benchmark_lib Threads::Threads)

# Legacy executables (from original files)
add_executable(udp_sender udp_sender.cpp)
target_link_libraries(udp_sender Threads::Threads)

add_executable(udp_receiver udp_receiver.cpp)
target_link_libraries(udp_receiver Threads::Threads)

# Install rules
install(TARGETS udp_sender_new udp_receiver_new udp_sender udp_receiver
    RUNTIME DESTINATION bin
)

install(TARGETS udp_benchmark_lib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Testing (optional)
enable_testing()

# Example test
add_executable(test_packet tests/test_packet.cpp)
target_link_libraries(test_packet udp_benchmark_lib)
add_test(NAME packet_test COMMAND test_packet)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    
    add_custom_target(doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# Package configuration
include(CPack)
set(CPACK_PACKAGE_NAME "udp-latency-benchmark")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance UDP latency benchmarking tool")
set(CPACK_GENERATOR "TGZ;ZIP")