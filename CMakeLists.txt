cmake_minimum_required(VERSION 3.5)
project(lora_phy LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_RUNNERS "Build runners" ON)

file(GLOB LORA_PHY_SOURCES CONFIGURE_DEPENDS src/phy/*.cpp)

add_library(lora_phy STATIC ${LORA_PHY_SOURCES})

target_include_directories(lora_phy PUBLIC include)

  # ensure headers like kissfft.hh are part of the target for IDEs
  target_sources(lora_phy PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include/lora_phy/kissfft.hh
  )

if(BUILD_RUNNERS)
    add_executable(lora_phy_vector_dump runners/lora_phy_vector_dump.cpp)
    target_link_libraries(lora_phy_vector_dump PRIVATE lora_phy)

    # Transmit runner producing IQ samples from a hex payload
    add_executable(tx_runner runners/tx_runner.cpp)
    target_link_libraries(tx_runner PRIVATE lora_phy)

    # Receive runner converting IQ samples back into payload bytes
    add_executable(rx_runner runners/rx_runner.cpp)
    target_link_libraries(rx_runner PRIVATE lora_phy)

endif()

if(BUILD_TESTS)
    enable_testing()

    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    list(REMOVE_ITEM TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp)

    # Copy test data files so the tests can be executed from the build
    # directory without relying on the source tree layout.  The tests load
    # profiles.yaml and various golden vectors using paths relative to the
    # current working directory (e.g. "tests/profiles.yaml").  When running
    # lora_phy_tests directly from the build directory these files would not
    # be found.  Replicate the expected directory structure in the binary tree
    # to make the resources available.
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/profiles.yaml
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/vectors
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(lora_phy_tests tests/test_main.cpp ${TEST_SOURCES} src/lorawan/lorawan.cpp src/lorawan/aes.c)
    target_link_libraries(lora_phy_tests PRIVATE lora_phy)

    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        set_source_files_properties(${test_src} PROPERTIES COMPILE_DEFINITIONS "main=${test_name}_main")
    endforeach()

    add_test(NAME lora_phy_tests COMMAND lora_phy_tests)
    set_tests_properties(lora_phy_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
